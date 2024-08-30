//In case you want to implement the shared memory IPC as a library...

#ifndef _SHM_CHANNEL_H_
#define _SHM_CHANNEL_H_

#include "gfserver.h"

typedef unsigned short ushort;

// typedef the gfs_send function as fileSendPtr
typedef ssize_t (*fileSendPtr)(gfcontext_t*, void*, size_t);
typedef ssize_t (*headerSendPtr)(gfcontext_t*, gfstatus_t, size_t);


//---------- Nomenclature Notes
// Client = process making the file request (i.e. webproxy)
// Server = process satisfying the file request (i.e. simplecached)

//---- SHM Memory segment related
#define SEG_NAME_SIZE	16				// max size for a segment name

//----- IPC related
// to provide for multiple unique messageQ IDs for mulitple threads
// - IPC_MSG_THREAD_FACT - number of MSG transactions possible per thread
#define IPC_MSG_THREAD_FACT		10000			
#define IPC_SEG_SIZE			1024			// default SHM segment size
#define IPC_SEG_NAME			"shm"			// segment base name
#define IPC_ERROR				-2				// error code
#define IPC_OK					0				// ok code		

#define MAX_CACHE_REQUEST_LEN 	80
//#define MAX_NAME_LEN		    80		// max file name length

typedef struct cacheArg
{
	int segSize;
	int threadID;
} cacheArg_t;


// a map of a virtual address to a segment name and message queue initial ID
typedef struct segment
{
	int   	    msgQInitVal;				// message queue initial ID for file-transfer
	char  	    segName[SEG_NAME_SIZE];	    // POSIX segment name
	size_t* 	virtAddr;					// processes virtual address
} segment_t;


//--- Initialization funcs for SHM
// must be called only by the client, i.e. webproxy
// segArray: array of segment_t containing the SHM pool
// numSeg:   the size of segArray
// segSize:  the SHM segment size
void SHM_AllocatePool( segment_t* segArray, int numSeg, int segSize ); 

//--- Frees objects that have kernel-life for client and server
void SHM_FreeMemory(int numSeg, segment_t* segArray);

//--- SHM_InitClient:
// segSize: SHM segment size being used
int SHM_InitClient( int segSize );	

//--- SHM_InitServer:
// segSize: SHM segment size being used
int SHM_InitServer( int* segSize );

//--- SHM_GetReqServer:
// va 		= virtual address
// keyPath  = path of the requested file
// msgQInit = the initial ID for the file-transfer message queue
// segSize  = the segment size being used (communicated earlier by webproxy)
int SHM_GetReqServer(size_t** va, char* keyPath, int* msgQInit, int segSize);

//--- SHM_GetReqServer:
// fd      = file descriptor for file being "transacted"
// va	   = virtual address
// segSize = size of the shared memory segment used for file transfer
// msgID   = the initial ID to use for the file-transfer message queue
int SHM_TransactServer( int fd, void* va, int segSize, int msgID );

//--- SHM_TransactClient:
// data       = a FILE handle pointer
// shmSeg     = segment_t struct containing SHM segment info
// reqFile    = the file being requested
// segSize    = the segment size
// rxFileSize = length of the received file
int SHM_TransactClient( FILE* data, const segment_t* shmSeg, const char* reqFile,
	int segSize, int* rxFileSize );


#endif



