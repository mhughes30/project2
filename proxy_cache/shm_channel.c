//In case you want to implement the shared memory IPC as a library...

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h> 
#include <sys/sem.h>
#include <semaphore.h>

#include "shm_channel.h"

//---------------- Type Defines --------------------//

typedef struct msg
{
    int  mtext;					// current byte length to be read
    char mseg[SEG_NAME_SIZE];	// current SHM segment name
} m_t;

//--- Message Queue Type: Used for passing the segment name only
typedef struct msgbuf 
{
    long    mtype;		// ID for this message type
    m_t	    mt;
} msg_t;

//--- Message Queue Type: Used for file transfer only
typedef struct msgbufsimple
{
    long    mtype;		// ID for this message type
	int     mtext;	
} msgFile_t;

//--- IPC Request - provides segnment name and file requested
// client process makes a request with this struct
typedef struct request
{
	char 	 fileName[MAX_CACHE_REQUEST_LEN];
} ipcRequest_t;

//--- IPC Response - provides the length of the file to be received
// server process responds with this to a client request
typedef struct 
{
	int fileLen;
} ipcResponse_t;

//---------------- Local Variables --------------------//
static int    clientMsgId       = 0;        // message queue IDs for pre-file transfer
static int    serverMsgId       = -1;
static int    clientMsgIdF      = 0;        // message queue IDs for file transfer server->client
static int    serverMsgIdF      = -1;
static int    clientMsgIdFRx    = 0;        // message queue IDs for file transfer client->server
static int    serverMsgIdFRx    = -1;

//#define IPC_SEM_NAME    "/semFile"
//#define IPC_SEM_PERMS   (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
//#define IPC_SEM_VAL     1

//-------- Message Variables -------//
#define MSG_KEY_FILE_RX	573 	// messageQ key
#define MSG_KEY_FILE	357 	// messageQ key
#define MSG_KEY		    1234	// messageQ key
//#define MSG_REQ		    11		// request message type
#define MSG_RESP_S	    22		// server response message type
//#define MSG_RESP_C	    33		// client response message type
//#define MSG_TRANS		44		// transaction message type
#define MSG_SEGSIZE     55		// for telling the segment size
#define MSG_SEGNAME	    66		// for telling the segment name
#define MSG_TRANS_RX	77		// transaction message type
static size_t MSGSZ     = sizeof(m_t);	// size of the message portion of message queue
static size_t MSGSZFILE = sizeof(int);	// size of the message portion of message queue

static int SHM_InitSharedMem( const char* segName, size_t segSize, size_t** addrOut );

pthread_mutex_t cMutex = PTHREAD_MUTEX_INITIALIZER;

//----------- SHM_AllocatePool -----------//
void SHM_AllocatePool( segment_t* segArray, int numSeg, int segSize )
{
	int  i = 0;
	int  shmSubIdx = 0;
	char subName[SEG_NAME_SIZE] = {0};
	
	for (i=0; i<numSeg; ++i)
	{
		//--- message Q initial value
		shmSubIdx = (i+1) * IPC_MSG_THREAD_FACT;
		segArray[i].msgQInitVal = shmSubIdx;
		//--- create the unique SHM Name for each segment		
		sprintf(subName, "%d", shmSubIdx);
		strcpy(segArray[i].segName, IPC_SEG_NAME);
		strcat(segArray[i].segName, subName);
		//--- create the shared memory
		SHM_InitSharedMem( segArray[i].segName, segSize, &segArray[i].virtAddr );
	}
}


//----------- SHM_FreeMemory -----------//
// function only called by Client - i.e. webproxy
void SHM_FreeMemory(int numSeg, segment_t* segArray)
{
	for (int i=0; i<numSeg; ++i)
	{
		munmap(segArray[i].virtAddr, IPC_SEG_SIZE);
		shm_unlink(segArray[i].segName);
	}		
	msgctl(clientMsgId, IPC_RMID, NULL);
    msgctl(clientMsgIdF, IPC_RMID, NULL);
    msgctl(clientMsgIdFRx, IPC_RMID, NULL);
}


//----------- SHM_InitSharedMem -----------//
// function only called by Client - i.e. webproxy
static int SHM_InitSharedMem( const char* segName, size_t segSize, size_t** addrOut )
{
	// create SHM
	int fd = shm_open( segName, (O_CREAT | O_RDWR), (S_IRUSR | S_IWUSR) );
	if (fd < 0)
	{
		fprintf(stderr, "shm_open() failed\n");
	}
	// set SHM size
	ftruncate(fd, segSize);
	// map process virtual memory to shared memory
	*addrOut = (size_t*)mmap(NULL, segSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	close(fd);	

	return IPC_OK;
}


//----------- SHM_InitClient -----------//
int SHM_InitClient( int segSize )
{
	msg_t  clientMsg     = {0};

	// client creates the Q - server just accesses it
	clientMsgId = msgget(MSG_KEY, IPC_CREAT | 0666);
	if (clientMsgId < 0)
	{
		fprintf(stderr, "SHM_InitClient error\n");
		return IPC_ERROR;
	}	
    clientMsgIdF   = msgget(MSG_KEY_FILE, IPC_CREAT | 0666);
    clientMsgIdFRx = msgget(MSG_KEY_FILE_RX, IPC_CREAT | 0666);
            
    //---- send a msg to server, telling segment size
	clientMsg.mtype = MSG_SEGSIZE;
	clientMsg.mt.mtext = segSize;
	msgsnd(clientMsgId, &clientMsg, MSGSZ, 0);

	return IPC_OK;
}


//----------- SHM_InitServer -----------//
int SHM_InitServer( int* segSize )
{
	msg_t  serverMsg     = {0};
        
	// Get the msg Q, and wait for client to create it first
	// Indicates that the client/webproxy is up-and-running
	while (serverMsgId < 0)
	{
		serverMsgId = msgget(MSG_KEY, 0666);
	}
    
    while (serverMsgIdF < 0)
	{
        serverMsgIdF = msgget(MSG_KEY_FILE, 0666);
    }

    while (serverMsgIdFRx < 0)
	{
        serverMsgIdFRx = msgget(MSG_KEY_FILE_RX, 0666);
    }

	//---- RCV message for segment size
	int rx = msgrcv(serverMsgId, &serverMsg, MSGSZ, MSG_SEGSIZE, 0);
	*segSize = serverMsg.mt.mtext;
	if (rx < 0)
	{
		fprintf(stderr, "msgrcv error\n");
		return IPC_ERROR;
	}	

	return IPC_OK;
}


//----------- SHM_GetReqServer -----------//
//static int segMsgIdCntr;
int SHM_GetReqServer(size_t** va, char* keyPath, int* msgQInit, int segSize)
{
	char segName[SEG_NAME_SIZE] = {0};
	int rx = 0;
	msg_t  			serverMsg   = {0};	// segment name message
	msgFile_t  		reqMsg     	= {0};	// misc. message
	ipcRequest_t	theReq 		= {0};	

	//---- 1a) wait for a segment name
   // pthread_mutex_lock(&cMutex);
	rx = msgrcv(serverMsgId, &serverMsg, MSGSZ, MSG_SEGNAME, 0);
   // pthread_mutex_unlock(&cMutex);
    memcpy(segName, serverMsg.mt.mseg, SEG_NAME_SIZE);
	*msgQInit = serverMsg.mt.mtext;
    
    printf("\n");

	//--- map the virtual memory
	int fd = shm_open( segName, (O_RDWR), (S_IRUSR | S_IWUSR) );
	if (fd < 0)
	{
		fprintf(stderr, "shm_open() failed\n");
	}
	*va = (size_t*)mmap(NULL, segSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);

	//---- 1b) send a RESP
	reqMsg.mtype = MSG_RESP_S;
	msgsnd(serverMsgId, &reqMsg, MSGSZFILE, 0);
	
	//---- 2a) wait for a request
	rx = msgrcv(serverMsgId, &reqMsg, MSGSZFILE,  *msgQInit, 0);
	if (rx < 0)
	{
		fprintf(stderr, "msgrcv error\n");
		return IPC_ERROR;
	}	
	
	//--- get the request
	memcpy(&theReq, *va, sizeof(ipcRequest_t));
	memcpy(keyPath, theReq.fileName, MAX_CACHE_REQUEST_LEN);
    fprintf(stderr, "msgID: %d segName: %s path: %s \n", *msgQInit, segName, keyPath);

	return 0;	
}


//----------- SHM_TransactServer -----------//
int SHM_TransactServer( int fd, void* va, int segSize, int msgID )
{
	char* buffer = (char*)malloc(segSize);
	size_t readLen   = 0;
	size_t bytesSent = 0;

	long* serverVaAddr = (long*)va;
	msgFile_t serverMsg = {0};

	ipcResponse_t theRep = {0};
    int transID = msgID;

	//--- determine file size
	int fileLen = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	//---- 2b) send RESP
	theRep.fileLen = fileLen;
	memcpy(serverVaAddr, &theRep, sizeof(ipcResponse_t));
	serverMsg.mtype = transID;
	serverMsg.mtext = fileLen;
	msgsnd(serverMsgId, &serverMsg, MSGSZFILE, 0);
	if (fileLen < 0)
	{
		free(buffer);
		return IPC_ERROR;
	}
    
    fprintf(stderr, "msgID: %d fileLen: %d \n", transID, fileLen);

    fprintf(stderr, "Begin: %d\n", transID);
	while (bytesSent < fileLen)
	{
		//--- read data
		readLen = read(fd, buffer, segSize);
		if (readLen == -1)
		{
            fprintf(stderr, "servErr: %d\n", transID);
			free(buffer);
			return IPC_ERROR;
		}		
        //fprintf(stderr, "s: %lu, %d\n", readLen, transID);
		//--- write to SHM
		memcpy(serverVaAddr, &buffer[0], readLen);	

		//--- send transaction message
		serverMsg.mtype = transID;
		serverMsg.mtext = readLen;
		msgsnd(serverMsgIdF, &serverMsg, MSGSZFILE, 0);
		bytesSent += readLen;

		//--- wait for reply
		msgrcv(serverMsgIdFRx, &serverMsg, MSGSZFILE, ++transID, 0);	
        
		++transID;		
	}
    fprintf(stderr, "End: %d\n", transID);

	free(buffer);

	return IPC_OK;
}


//----------- SHM_TransactClient -----------//
int SHM_TransactClient( FILE* data, const segment_t* shmSeg, const char* reqFile, 
	int segSize, int* rxFileSize )
{
	char* buffer = (char*)malloc(segSize);
	long* va = (long*)shmSeg->virtAddr;

	int transID  = shmSeg->msgQInitVal;

	ipcRequest_t  theReq = {0};	

	msg_t     segMsg    = {0};	// for segment name
	msgFile_t clientMsg = {0};	// for other transactions
	    
	//----1a) populate and SEND segment name and file msgID
	segMsg.mtype    = MSG_SEGNAME;  
	segMsg.mt.mtext = transID;                              // file message ID
	memcpy(segMsg.mt.mseg, shmSeg->segName, SEG_NAME_SIZE); // segment name
        
    pthread_mutex_lock(&cMutex);
	msgsnd(clientMsgId, &segMsg, MSGSZ, 0);
    
	//---- 1b) wait for RESP
	msgrcv(clientMsgId, &clientMsg, MSGSZFILE, MSG_RESP_S, 0);
    pthread_mutex_unlock(&cMutex);
    
	//---- 2a) populate and SEND request    
	memcpy(theReq.fileName, reqFile, MAX_CACHE_REQUEST_LEN);
	memcpy(va, &theReq, sizeof(ipcRequest_t));
	clientMsg.mtype = transID;
    
    //pthread_mutex_lock(&cMutex);
	msgsnd(clientMsgId, &clientMsg, MSGSZFILE, 0);

	//---- 2b) wait for RESP
	int rx = msgrcv(clientMsgId, &clientMsg, MSGSZFILE, transID, 0); 
    //pthread_mutex_unlock(&cMutex);
    
	if ( (rx < 0) || (clientMsg.mtext < 0) )
	{
		free(buffer);
		fprintf(stderr, "reqFile: %s\n", reqFile);
		fprintf(stderr, "msgrcv error, rx=%d, mtext=%d\n", rx, clientMsg.mtext);
		return IPC_ERROR;
	}
        
	//---- Grab RESP which contains file length
	*rxFileSize = clientMsg.mtext;
    
    fprintf(stderr, "msgID: %d fileLen: %d file: %s \n", transID, *rxFileSize, theReq.fileName);
    
    fprintf(stderr, "client: %d\n", transID);
	//---- Pass the data
	int rxBytes  = 0;
	int curBytes = 0;
	while ( rxBytes < *rxFileSize )
	{
		//--- wait for TRANS message
		msgrcv(clientMsgIdF, &clientMsg, MSGSZFILE, transID, 0);
        
		//--- grab the data
		curBytes = clientMsg.mtext;
        //fprintf(stderr, "c: %d, %d\n", curBytes, transID);
		if (curBytes < 0)
		{
			free(buffer);
            fprintf(stderr, "clientErr: %d\n", transID);
			return IPC_ERROR;
		}
		memcpy( &(buffer[0]), va, curBytes );

		//--- write to the file
		fwrite( &(buffer[0]), 1, curBytes, data );

		//--- reply that ready for next message
		clientMsg.mtype = ++transID;
		msgsnd(clientMsgIdFRx, &clientMsg, MSGSZFILE, 0);
        
		//--- increment buffer variables
		rxBytes += curBytes;
		++transID;
	}
    
	fprintf(stderr, "client end..\n");
	free(buffer);

	return IPC_OK;
}
















