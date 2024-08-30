#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "shm_channel.h"
#include "gfserver.h"

#define BUFF_SIZE			4096		// temporary buffer size

extern segment_t* shmSegments;


pthread_mutex_t wMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t tMutex = PTHREAD_MUTEX_INITIALIZER;


//----------------- shmqueue_t ------------------//
// items are added at the rear and poppoed off of the front
typedef struct queue
{
	int*    shmIdx;	// the queued item: index to the SHM Pool
	int	    front;
	int 	rear;
	int	    numItem;
	int     capacity;
} shmqueue_t;
static shmqueue_t theQ;

//---- SHM_Q prototypes - shared pool of memory resources
static int 	SHM_Q_GetNextIdx( int curIdx, int size );
static int 	SHM_Q_Deq( void );
static int 	SHM_Q_isEmpty( void );
static int 	SHM_Q_isFull( void );
void SHM_Q_Init( int size );
void SHM_Q_Cleanup( void );
void SHM_Q_Enq( int shmIdx );

//--------------- handle_with_cache ------------------//
ssize_t handle_with_cache(gfcontext_t* ctx, char* path, void* arg)
{
	int      fileLen;
	size_t   bytesSent;
	ssize_t  readLen;
	ssize_t  writeLen;
	cacheArg_t cacheSeg;

	int shmIdx  = 0;	// index of the SHM pool to use
	int shmRes  = 0;
	int segSize = 0;

	char dataBuff[BUFF_SIZE]    = {0};
	char fullPath[MAX_CACHE_REQUEST_LEN] = {0};

	// temporary file for buffering the cached file
	FILE* tempFile;
	char  tempFileName[16] = {0};

	segment_t curSegment = {0};
	memcpy(&cacheSeg, arg, sizeof(cacheArg_t));
    memcpy(fullPath, path, MAX_CACHE_REQUEST_LEN );
	segSize = cacheSeg.segSize;
	
    // open the temporary file
	sprintf(tempFileName, "%d", cacheSeg.threadID);	
	tempFile = fopen(tempFileName, "wb+");

	//---- Obtain a Free Segment
    while (SHM_Q_isEmpty());	
    pthread_mutex_lock(&wMutex);
	shmIdx = SHM_Q_Deq();
    memcpy(&curSegment, &shmSegments[shmIdx], sizeof(segment_t));
    pthread_mutex_unlock(&wMutex);	
    
    printf("\n\n");
	fprintf(stderr, "threadID: %d segName: %s path: %s \n", cacheSeg.threadID, curSegment.segName, path);
    
	//---- Read the file from cache
  //  pthread_mutex_lock(&tMutex);
	shmRes = SHM_TransactClient( tempFile, &curSegment, fullPath, segSize, &fileLen );
  //  pthread_mutex_unlock(&tMutex);	
	
	//---- enqueue the used segment now (now free)
    fprintf(stderr, "threadID: %d segName: %s fileLen: %d \n", cacheSeg.threadID, curSegment.segName, fileLen);
    
    while (SHM_Q_isFull());	
    pthread_mutex_lock(&wMutex);
	SHM_Q_Enq(shmIdx);	
    pthread_mutex_unlock(&wMutex);	
		
	if( shmRes == IPC_ERROR )
	{
		fprintf(stderr, "SHM_TransactClient() failed  ");
		return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
	}
    
    printf("\n\n");

	//---- move file pointer to front of file
	fseek(tempFile, 0, SEEK_SET);

	gfs_sendheader(ctx, GF_OK, fileLen);

	//---- Sending the file contents chunk by chunk.
	bytesSent = 0;
	while (bytesSent < fileLen)
	{
		readLen = fread(dataBuff, 1, BUFF_SIZE, tempFile);
		if (readLen <= 0)
		{
			fprintf(stderr, "handle_with_cache read error, %zd, %zu, %d", readLen, bytesSent, fileLen );
			return SERVER_FAILURE;
		}
		writeLen = gfs_send(ctx, dataBuff, readLen);
		if (writeLen != readLen)
		{
			fprintf(stderr, "handle_with_cache write error");
			return SERVER_FAILURE;
		}
		bytesSent += writeLen;
	}

	fclose(tempFile);

	return bytesSent;
}


//----------------------------------------------------------------------//
//-------------- Shared Memory Pool Queue Implementation ---------------//
//----------------------------------------------------------------------//
//-------- SHM_Q_GetNextIdx --------//
int SHM_Q_GetNextIdx( int curIdx, int size )
{
	return ( (curIdx + 1) % size );
}

//-------- SHM_Q_Init --------//
void SHM_Q_Init( int size )
{
	theQ.front	  = 0;
	theQ.rear  	  = size - 1;
	theQ.capacity = size; 
	theQ.numItem  = 0;

	theQ.shmIdx = (int*)malloc( size * sizeof(int) );
}

//-------- SHM_Q_Enq --------//
void SHM_Q_Enq( int shmIdx )
{
	theQ.rear = SHM_Q_GetNextIdx( theQ.rear, theQ.capacity );
	theQ.shmIdx[theQ.rear] = shmIdx;
	theQ.numItem++;
}

//-------- SHM_Q_Deq --------//
int SHM_Q_Deq( void )
{
	int idx    = theQ.front;
	theQ.front = SHM_Q_GetNextIdx(theQ.front, theQ.capacity);
	theQ.numItem--;

	return idx;
}

//-------- SHM_Q_isEmpty --------//
int SHM_Q_isEmpty( void )
{
	if (theQ.numItem == 0)
		return 1;

	return 0;
}

//-------- SHM_Q_isFull --------//
int SHM_Q_isFull( void )
{
	if (theQ.numItem >= theQ.capacity)
		return 1;

	return 0;
}

//-------- QueueCleanup --------//
void SHM_Q_Cleanup( void )
{
	free( theQ.shmIdx );
}
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//
//----------------------------------------------------------------------//


