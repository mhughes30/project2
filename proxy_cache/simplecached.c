#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>

#include "shm_channel.h"
#include "simplecache.h"

#if !defined(CACHE_FAILURE)
	#define CACHE_FAILURE (-1)
#endif 


#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -h                  Show this help message\n"                              \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1000)\n"		\

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = 
{
	{"cachedir",           required_argument,      NULL,           'c'},
	{"help",               no_argument,            NULL,           'h'},
	{"nthreads",           required_argument,      NULL,           't'},
	{NULL,                 0,                      NULL,             0}
};

//----------------- Queue Type ------------------//
// items are added at the rear and poppoed off of the front
typedef struct queue
{
	char**      keys;		// path/key name for requested file
	size_t**    va;			// virtual address being used
	int*        msgID;		// starting ID for file transfer messages
	int		    front;
	int 		rear;
	int		    numItem;
	int   	    capacity;
} queue_t;
static queue_t theQ;

static int 	Q_GetNextIdx( int curIdx, int size );
static void Q_Init( int size );
static void Q_Enq( char* key, int msgID, size_t* addr );
static int 	Q_Deq( void );
static int 	Q_isEmpty( void );
static int  Q_isFull( void );


//------------ Global PThread Resources -------------//
pthread_mutex_t gMutex    = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  gCond     = PTHREAD_COND_INITIALIZER;
pthread_t* 		workerThreads;
int*		  	threadIDs;

//---- boss-worker functions
void *workerFunc(void *threadArgument);
void  bossFunc(void);


static void Usage();
static void _sig_handler(int signo);
static void ProcessCmdArgs(int argc, char* argv[]);

//------------ Local Variables ------------//
static int   nthreads 	= 1;
static char* cachedir   = "locals.txt";
static int   segSize 	= 0;


//----------------- main -------------------//
int main(int argc, char **argv) 
{
	int i = 0;

	ProcessCmdArgs( argc, argv );

	if ( (nthreads < 1) || (nthreads > 1024) ) 
	{
		nthreads = 1;
	}

	if (signal(SIGINT, _sig_handler) == SIG_ERR)
	{
		fprintf(stderr,"Can't catch SIGINT...exiting.\n");
		exit(CACHE_FAILURE);
	}

	if (signal(SIGTERM, _sig_handler) == SIG_ERR)
	{
		fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
		exit(CACHE_FAILURE);
	}

	//---- Initializing the cache and boss-worker Q
	simplecache_init(cachedir);
	Q_Init(nthreads + 2); 
	fprintf(stderr,"nthreads: %d\n", nthreads);

	//---- Initialize SHM
	SHM_InitServer( &segSize );

	//---- Create the Worker Threads
	workerThreads = (pthread_t*)malloc( nthreads*sizeof(pthread_t) );
	threadIDs	  = (int*)malloc( nthreads*sizeof(int) );
	for (i=0; i<nthreads; ++i)
	{
		threadIDs[i] = i;
		pthread_create( &workerThreads[i], NULL, workerFunc, &threadIDs[i] );	
	}	

	//---- Run the Boss Func
	bossFunc();

	//---- destroy the cache
	simplecache_destroy();
	free(workerThreads);
	free(threadIDs);
}


//------------------ bossFunc ----------------------//
void bossFunc(void)
{
	char key[MAX_CACHE_REQUEST_LEN] = {0};
	size_t* virtAddr = 0;
	int msgQInit 	 = 0;

	while(1)
	{
		//---- Get the file-transfer request       
		SHM_GetReqServer(&virtAddr, key, &msgQInit, segSize);

		//---- enqueue the request
		pthread_mutex_lock(&gMutex);
        fprintf(stderr, "bossQ\n");
		Q_Enq(key, msgQInit, virtAddr);
		pthread_mutex_unlock(&gMutex);

		while ( Q_isFull() );
		pthread_cond_signal(&gCond);
	}
}


//------------------ workerFunc ----------------------//
void *workerFunc(void *threadArgument)
{
	int  tID 	  = *( (int*)threadArgument );
	int  fileDesc = 0;
	int  keyIdx   = 0;
	char buffer[MAX_CACHE_REQUEST_LEN] = {0};
	size_t* va = NULL;
	int msgID = 0;

	fprintf(stderr, "Thread #%d\n", tID);

	while(1)
	{
		pthread_mutex_lock(&gMutex);
		
		while ( Q_isEmpty() )
			pthread_cond_wait( &gCond, &gMutex );

		//---- dequeue a request
		keyIdx = Q_Deq();
		memcpy(buffer, theQ.keys[keyIdx], MAX_CACHE_REQUEST_LEN);
		va 		= theQ.va[keyIdx];
		msgID	= theQ.msgID[keyIdx];
        fprintf(stderr, "threadQ %d\n", tID);
		pthread_mutex_unlock(&gMutex);	

		//---- get the file from the cache
		fileDesc = simplecache_get(buffer);
		if (fileDesc == CACHE_FAILURE)
		{
			fprintf(stderr, "simpleCache_get() error: %s\n", buffer);
		}
		
		//---- transfer the file 
        fprintf(stderr, "SHM_transact %s\n", buffer);
		SHM_TransactServer( fileDesc, va, segSize, msgID );
	}
	
	pthread_exit(0);
}


//------------------ ProcessCmdArgs ----------------------//
static void ProcessCmdArgs(int argc, char* argv[])
{
	int option = 0;

  	// Parse and set command line arguments
	while ((option = getopt_long(argc, argv, "t:c:h", gLongOptions, NULL)) != -1) 
	{
		switch (option) 
		{
		case 't': // thread-count
			nthreads = atoi(optarg);
			break;   
		case 'c': //cache directory
			cachedir = optarg;
			break;
		case 'h': // help
			Usage();
			exit(0);
			break;    
		default:
			Usage();
			exit(1);
		}
	}
}


//----------------------------------------------------------------------//
//------------------------ Queue Implementation ------------------------//
//----------------------------------------------------------------------//
//-------- Q_GetNextIdx --------//
// used internally by Q
int Q_GetNextIdx( int curIdx, int size )
{
	return ( (curIdx + 1) % size );
}

//-------- Q_Init --------//
void Q_Init( int size )
{
	int i = 0 ;

	theQ.front	  = 0;
	theQ.rear  	  = size - 1;
	theQ.capacity = size; 
	theQ.numItem  = 0;

	theQ.keys  = (char**)malloc(size*sizeof(char*));
	theQ.va    = (size_t**)malloc(size*sizeof(size_t*));
	theQ.msgID = (int*)malloc(size*sizeof(int));

	for (i=0; i<size; ++i)
	{
		theQ.keys[i] = (char*)malloc(MAX_CACHE_REQUEST_LEN*sizeof(char));
	}
}

//-------- Q_Enq --------//
void Q_Enq( char* key, int msgID, size_t* addr )
{
	theQ.rear = Q_GetNextIdx( theQ.rear, theQ.capacity );
    memcpy( theQ.keys[theQ.rear], key, MAX_CACHE_REQUEST_LEN ); 
	theQ.va[theQ.rear]    = addr;
	theQ.msgID[theQ.rear] = msgID; 
	theQ.numItem++;
}

//-------- Q_Deq --------//
int Q_Deq( void )
{
	int idx    = theQ.front;
	theQ.front = Q_GetNextIdx(theQ.front, theQ.capacity);
	theQ.numItem--;

	return idx;
}

//-------- Q_isEmpty --------//
int Q_isEmpty( void )
{
	if (theQ.numItem == 0)
		return 1;

	return 0;
}

//-------- Q_isFull --------//
int Q_isFull( void )
{
	if (theQ.numItem >= theQ.capacity)
		return 1;

	return 0;
}
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//


//----------------- Usage -------------------//
static void Usage() 
{
	fprintf(stdout, "%s", USAGE);
}


//----------------- _sig_handler -------------------//
static void _sig_handler(int signo)
{
	if (signo == SIGINT || signo == SIGTERM)
	{
		/* Unlink IPC mechanisms here*/
		// Note: client unlinks the IPC mechanisms
		exit(signo);
	}
}

