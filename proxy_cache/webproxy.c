#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h> 
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "shm_channel.h"
#include "gfserver.h"
                                  
#define MAX_N_PENDING	12
                             
#define USAGE                                                                  \
"usage:\n"                                                                     \
"  webproxy [options]\n"                                                       \
"options:\n"                                                                   \
"  -p [listen_port]    Listen port (Default: 8080)\n"                          \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1000)\n"       \
"  -s [server]         The server to connect to (Default: Udacity S3 instance)\n"\
"  -n [# cache seg]    Num cache segments (Default: 1)\n"							 \
"  -z [seg size]       Cache segment size (Default: 1024)\n"						 \
"  -h                  Show this help message\n"                               \
"special options:\n"                                                           \
"  -d [drop_factor]    Drop connects if f*t pending requests (Default: 5).\n"


/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = 
{
	{"cache segments", required_argument,		 NULL,			 'n'},
	{"segment-size",   required_argument, 		 NULL,			 'z'},
	{"port",           required_argument,        NULL,           'p'},
    {"server",         required_argument,        NULL,           's'},         
    {"thread-count",   required_argument,        NULL,           't'},
    {"help",           no_argument,              NULL,           'h'},
    {NULL,             0,                        NULL,             0}
};

extern ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg);
extern ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg);

//---- Misc prototypes
static void  ProcessCmdArgs(int argc, char* argv[]);
static void  _sig_handler(int signo);
static void  Usage();

extern void SHM_Q_Init( int size );
extern void SHM_Q_Cleanup( void );
extern void SHM_Q_Enq( int shmIdx );


//---- static Variables
static cacheArg_t* cacheSeg = 0;
static gfserver_t gfs;
static unsigned short port = 8080;
static unsigned short numWorkerThreads = 4;
static unsigned short numCacheSeg      = 4;
static unsigned int	 cacheSegBytes 	= IPC_SEG_SIZE;
static char *server = "s3.amazonaws.com/content.udacity-data.com";

//---- global Variables
segment_t* shmSegments = 0;


//--------------------- MAIN ---------------------//
int main(int argc, char **argv) 
{
	int i = 0;	

  	if (signal(SIGINT, _sig_handler) == SIG_ERR)
	{
	   fprintf(stderr,"Can't catch SIGINT...exiting.\n");
	   exit(SERVER_FAILURE);
	}

  	if (signal(SIGTERM, _sig_handler) == SIG_ERR)
	{
	   fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
		exit(SERVER_FAILURE);
  	}

	ProcessCmdArgs(argc, argv);

	fprintf(stderr,"numThreads: %d\n", numWorkerThreads);

	if (!server) 
	{
	   fprintf(stderr, "Invalid (null) server name\n");
	   exit(1);
  	}
  
  	//---- SHM initialization
	shmSegments = malloc( numCacheSeg * sizeof(segment_t));
	SHM_AllocatePool( shmSegments, numCacheSeg, cacheSegBytes );
	//---- Init the Free Segment Q
	SHM_Q_Init( numCacheSeg );
	for (i=0; i<numCacheSeg; ++i)
	{
		SHM_Q_Enq(i);
	}
	//---- client init
	SHM_InitClient(cacheSegBytes);	

	printf("client init done\n");

  	//---- Initializing server
  	gfserver_init(&gfs, numWorkerThreads);

  	//---- Setting options
  	gfserver_setopt(&gfs, GFS_PORT, port);
  	gfserver_setopt(&gfs, GFS_MAXNPENDING, MAX_N_PENDING);
  	gfserver_setopt(&gfs, GFS_WORKER_FUNC, handle_with_cache);

	cacheSeg = malloc(numWorkerThreads * sizeof(cacheArg_t));
	  
	for(i = 0; i < numWorkerThreads; i++)
	{
		cacheSeg[i].segSize = cacheSegBytes;
		cacheSeg[i].threadID = i;
		gfserver_setopt(&gfs, GFS_WORKER_ARG, i, &cacheSeg[i]);
	}

  	//---- Loops forever
  	gfserver_serve(&gfs);
	SHM_FreeMemory(numCacheSeg, shmSegments);
	free(cacheSeg);
}


//--------------------- _sig_handler ---------------------//
static void _sig_handler(int signo)
{
	if (signo == SIGINT || signo == SIGTERM)
	{
		gfserver_stop(&gfs);
		SHM_FreeMemory(numCacheSeg, shmSegments);
	   exit(signo);
  	}
}


//------------ Usage ---------------//
static void Usage() 
{
	fprintf(stdout, "%s", USAGE);
}


//------------------ ProcessCmdArgs ----------------------//
static void ProcessCmdArgs(int argc, char* argv[])
{
	int option = 0;

  	// Parse and set command line arguments
  	while ( (option = getopt_long(argc, argv, "n:z:p:s:t:h", gLongOptions, NULL)) != -1 ) 
	{
	   switch (option) 
		{
	     case 'p': // listen-port
	        port = atoi(optarg);
	        break;
	     case 't': // thread-count
			  numWorkerThreads = atoi(optarg);
       	  break;
        case 's': // file-path
        	  server = optarg;
           break;                                          
        case 'h': // help
           Usage();
           exit(0);
           break;  
		  case 'n':	// number of cache segments
		     numCacheSeg = atoi(optarg);
			  break;
		  case 'z':
			  cacheSegBytes = atoi(optarg);
			  break;     
        default:
           Usage();
           exit(1);
		}
	}
}






