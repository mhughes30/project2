
# Project README file

Author: Mike Hughes (gte769h)

This is the README file for project 3: IPC - Inter-Process Communication

## Project Description
This project consists of 3 parts: part 1, part 2, and extra credit.


### Part 1: Create proxy server with libcurl. 

- __Summary Description__

Part 1 primarily consisted of learning how to use the libcurl easy interface. Libcurl is a multiprotocol file transfer library 
that accesses files via a URL. It is a well-documented and easy-to-use library. 

The goal of this project was to modify the GETFILE server, so that it acquires the desired file via a URL, rather than via disk. 
This basically involved creating a callback function (the handler passed to the GETFILE server) that utilizes libcurl to acquire
the file over a network. The callback function was implemented in handler_for_curl.c, which is very similar to the provided example handler_for_file.c.
The primary difference between the two is simply how the file is acquired; via disk or via URL.

The function curl_global_init() must be called at least once prior to using libcurl. However, this function is not thread safe. Therefore, I created a global
function, InitCurl() which is called in main() within webproxy.c, prior to the creation of any threads. 

All other libcurl function calls appear within RunCurl(), which is essentially a replacement for the open() call used in handler_for_file.c for acquiring the
file from disk. Since RunCurl() uses libcrul, it instead acquires the file via its URL. RunCurl() is a static function. 

The following options were set for libcurl, with explanations provided:

1) CURL_OPT_URL 
- required

2) CURLOPT_HEADER
- Set to 0: This prevents the html header from being included in the acquired file.

3) CURLOPT_WRITEFUNCTION
- I didn't use this option, since all I wanted to do was read and store the file contents in a FILE handler (FILE*)

4) CURLOPT_WRITEDATA
- The argument here is the FILE handle (FILE*) used for storing the files contents. 

5) CURLOPT_USERAGENT
- Set to "libcurl-agent/1.0": libcurl documentation stated that sometimes network servers require this information, so I set this option. 

6) CURLOPT_VERBOSE
- Set to 0: I had it set to 1 to provide useful debug information from libcurl, prior to submitting the project. 

7) CURLOPT_FAILONERROR
- Set to 1: I set this to once, because without this curl_easy_perform() would not fail if a file was not found (404 error). 

- __What created problems for you?__

I didn't have any serious problems with this portion of the project. I mostly just had to familiarize myself with libcurl.
Prior to realizing that curl_init_global() was not thread safe, I placed it inside of RunCurl(), which is a function called by each thread. 
This causes conflict leading to errors when multiple threads were used for the webproxy.c. Therefore, I created a global function InitCurl(), 
which is called prior to any thread creation in webproxy.c to correct this issue. 

- __What tests would you have added to the test suite?__

I personally tested for the following: file that don't exist, large files, small files, and several combinations of threads for both the 
webproxy and client_download. The test suite essentially tested for all of the same things. Therefore, there is nothing I would add to the
test suite. 

- __If you were going to do this project again, how would you improve it?__

Currently, the webproxy has 2 handles, one for a file located locally on disk, and another for a file located remotely via a network. 
It would be interesting to have only 1 file handle, that handles both situations. Perhaps the handler could example the directory name to 
see if it is a URL. Or, perhaps the handler could see if the directory exists first. If so, it reads the file from disk. If not, it uses libcurl. 

Also, I could have possibly used the CURLOPT_WRITEFUNCTION to create a write callback. These callback would be passed RunCurl() so that curl could obtain the file and send it using gfs_send and gfs_sendheader to the GFS Client. 
However, since this project worked fine as is, I didn't pursue this due to added complexity. 

- __If you didn't think something was clear in the documentation, what would you write instead?__

I think the documentation was clear. 


### Part 2: Create a proxy server using shared-memory IPC. 

- __Summary Description__

Part 2 primarily consisted of learning how to use shared memory (SHM) and IPC mechanisms (IPC). We had the option of using either SysV or POSIX. I chose POSIX for the SHM API, because the implementation looked cleaner and easier to use. 
However, I uses SysV for the message queue API because it looked cleaner than the POSIX version. 

The goal of this project was to modify the GETFILE server, so that it acquires the desired file via a "cache", rathern than via a URL or directly via disk. 
The webproxy (using GETFILE server commands) receives requests from the GETFILE client. 
The webproxy then serves the file request via communicating with the simplecached process, via SHM and IPC. SHM is used for transfering the file itself, and IPC is used for process synchronization, as well as transferring key information such 
as the message queue ID to use (transID in the code), and the SHM segment name to use for file transfer. 
The simplecached process checks if the requested file is in its cache. If so, it transfers the file; otherwise, it replies with an error, indicating the file was not found. 

The roles and responsibilities of each portion of the project (that I edited) are described here:

** webproxy.c **
* Runs the GETFILE server (using gfserver interface). This is responsible for servicing file requests from the GETFILE client. 
* Creates the threads used for the GETFILE server.
* Creates and destroys a pool of SHM resources, by calling SHM_AllocatePool.
* Creates a queue, shmqueue_t, used for obtaining a free SHM index in the SHM pool.
* Initializes the SHM Client (SHM_InitClient), which in turn creates the message queue (msgget), sends a message to the SHM Server indicating the SHM segment size in bytes, and waking up the simplecached process.
* Creates a cacheArg_t argument to use with handle_with_cache for each server thread. The cacheArg_t argument contains the threadID and the SHM segment size.

** simplecached.c **
* This is the cache process, servicing file requests from webproxy.
* Initializes the simplecahce by calling simplecache_init
* Initializes the boss-worker request queue, used for passing file requests from the boss thread to the worker threads.
* Runs the boss thread, bossFunc. The bossFunc calls SHM_GetReqServer, which acquires the file requests. The bossFunc then places the request on a request queue for each worker to access. 
* Creates all of the worker threads. The worker threads obtain requests from the request queue. It calls simplecahce_get to see obtain the file descriptor of the cached file. It the performs the file transfer transacton 
by calling SHM_TransactServer.
* * Initializes the SHM Server (SHM_InitServer), which calls msgget for each of the 3 message queues already created by SHM_InitClient. It then receives a message from the SHM client, indicating the SHM segment size to use. 

** handle_with_cache.c **
* This constains the GETFILE server handle, handle_with_cache, which processes the file requests by communicating with the cache process via SHM and IPC mechanisms.
* Opens a temporary file (using the threadID as the file name), which has its handle passed to SHM_TransactClient. This temporary file is used for acquiring the data from the cache process.
* Calls SHM_TransactClient, which acquires the cached file. 
* It serves the GETFILE server request, by sending the cached file chunk-by-chunk using gfs_send. 

** shm_channel.c / shm_channel.h **
* Povides an abstracted interface for performing the cached file transfer, using a client/server abstraction. 
* The goal of this was to simplify the SHM and IPC mechanisms from the perspective of handle_with_cache and simplecached. 
* In this case, the SHM client is the webproxy (with handle_with_cache), and the SHM server is simplecached. 
* The client calls SHM_AllocatePool (allocates SHM memory pool), SHM_FreeMemory (frees SHM memory pool and IPC), SHM_InitClient (initializes the SHM client), and SHM_TransactClient (performs the cached file transaction).
* The server calls SHM_InitServer (initializes the SHM server), SHM_GetReqServer (acquires a request from the SHM client), and SHM_TransactServer (performs the cahced file transaction).

I developed a library for performing SHM and IPC, which is included in shm_channel.c and shm_channel.h. This library handles all of the SHM details and the IPC synchronization mechanisms.
This was the bulk of the development for Part 2 of this project. The rest of the code (handle_with_cache, webproxy Here, and even simlecached) was very similar to Part 1. Here, I detail how the SHM library works. 

*** SHM Library structs ***

Several struct types were developed for use with the SHM library, and are described here. 

** segment_t **

 ```c 
typedef struct segment
{
	int   	 msgQInitVal;				// message queue initial ID for file-transfer
	char  	 segName[SEG_NAME_SIZE];	// POSIX segment name
	size_t*  virtAddr;					// processes virtual address
} segment_t;
```
The segment_t struct describes an individual index of the SHM memory pool. It contains a virtual-address corresponding to the SHM segment name, and an initial ID for the IPC message which is the agreed upon ID for the associated SHM 
segment name, to use for the IPC syncrhonization. Each server thread using the handle_with_cache handle, and passes a segment_t struct to SHM_TransactClient for file transfer purposes. This allows for each server thread and cahce
thread combination (combination meaning the 2 threads that are sending/receiving the same file) to use the same SHM segment, and have the same message queue IDS for each step of the cached file transfer, and preliminary synchronization. 

** cacheArg_t **

 ```c 
typedef struct cacheArg
{
	int segSize;	// segment size in bytes
	int threadID;	// the thread ID
} cacheArg_t;
```
The cacheArg_t struct is the argument used for handle_with_cache. It contains the SHM segment size and the current threads ID. The segSize is required so that handle_with_cache can pass the segment size to the SHM client library (SHM_TransactClient). 
The threadID is required for creating a temporary file whose handle is passed to SHM_TransactClient. This allows each GFS server thread to have its own separate temporary file. I found that using a single temporary file worked also, but I thought perhaps
this lead to serialization of the threads. Therefore, I made each thread have its own separate temporary file. 

** ipcRequest_t **

 ```c 
typedef struct request
{
	char 	 fileName[MAX_NAME_LEN];
} ipcRequest_t;
```
This struct was for passing the file request via shared memory. It just contains the path of the requested file to be passed to the cache server. 

** ipcResponse_t **

```c 
typedef struct 
{
	int fileLen;
} ipcResponse_t;
```
This struct is for sending the response in regard to the file request from the cache server to the webproxy. If fileLen is positive it is the length of the cached file in bytes. An error is indicated with a negative number 
if the cached file was not found. This allows the webproxy to know that the file was not found. 

** msg_t **

 ```c 
typedef struct msg
{
     int  mtext;					// initial messageQ Id information
	 char mseg[SEG_NAME_SIZE];		// current SHM segment name
} m_t;
typedef struct msgbuf 
{
    long  mtype;		// ID for this message type
	 m_t  mt;
} msg_t;
```
The msg_t struct is used solely for send the segment name and initial messageQ Id information for the file transfer. This struct is passed as a message between the webproxy and the cache process. This tells the cache progress what segment name to use for 
the file transaction. 

** msgFile_t **

 ```c 
typedef struct msgbufsimple
{
   	long mtype;		// ID for this message type
	int  mtext;		// context-specific purpose - most frequently is length in bytes
} msgFile_t;
```
The msgFile_t struct is used for all message other than the segment name message. It is at least 16 bytes smaller than the msg_t struct. This is useful, because there can be a lot of messages passed during the file transfer process. The use of mtext 
depends on the context. For some messages (just a response for example), it is not used, and for others it is a length in bytes. 

*** SHM Library Functions ***

Several functions were developed for use with the SHM library, and are described here. 

1. ```c void SHM_AllocatePool( segment_t* segArray, int numSeg, int segSize ) ```
This function is called by webproxy to allocated a shared memory pool. It allocates the segments and places the segment information inside segArray. The arguments numSeg and segSize are the number of segments and size of the segments. 
This function internally calls the local function SHM_InitSharedMem. It also creates a unique message queue ID, corresponding to the unique SHM segment name. 

2. ```c static int SHM_InitSharedMem( const char* segName, size_t segSize, size_t** addrOut ) ```
This function is only called by SHM_AllocatePool. This function allocates and maps a single SHM segment. It calls shm_open(), ftruncate(), and mmap() to do so. 

3. ```c void SHM_FreeMemory(int numSeg, segment_t* segArray) ```
This function is called by webproxy to free the SHM and IPC memory. It calls munmap() to unmap the virtual address from the SHM segment, shm_unlink() to close each SHM segment, and msgctl() to remove teh message queue ID. 

4. ```c int SHM_InitClient( int segSize ) ```
This is called by webproxy. It creates a message queue using msgget() with the IPC_CREAT argument. It also sends a message to the cache process, telling it the SHM segment size to use. 

5. ```c int SHM_InitServer( int* segSize ) ```
This is called by simplecached. It opens the message queue corresponding to the webproxy-created message queue. If simplecached is started before webproxy, it waits for the message queue to be created by webproxy before proceeding. 
It also receives the segment size from webproxy, and updates the segSize argument with this value. 

6. ```c int SHM_GetReqServer(size_t** va, char* keyPath, int* msgQInit, int segSize) ```
This is called by the simplecached (SHM server) boss thread. It receives the segment name being used for this transaction via an IPC message, opens the already-created segment, and maps the segment to simple caches virtual address space. 
It then sends a response indicating it received the segment name IPC message, and then receives a file request IPC message. From this request, it extracts the requested file path (keyPath), the virtual address used for this transaction (va),
 the initial message queue ID used for the file-transfer process (msgQInit). The boss thread then adds this information to the worker queue.

7. ```c int SHM_TransactServer( int fd, void* va, int segSize, int msgID ) ```
This processes the file transfer request for simplecached (SHM server), after the request has been received. Its inputs are the file descriptor obtained from simplecache_get() (fd), the virtual address to use (va), the size of the SHM segment (segSize), 
 and the initial message queue ID used for the file transfer portion. It determines the file length, and sends a response to the file-transfer request with the file length as an argument. It then waits for a response from webproxy (the SHM client).
Then it proceeds with initiating the file transfer in a while() loop. It reads up to segSize bytes from the file, copies it to the SHM, and then sends an IPC message with the byte length of the current SHM transaction, and then waits for a response before 
sending more data. Once the number of bytes send is equal to the file length, the while() loop exits. The worker threads call this function.

8. ```c int SHM_TransactClient( FILE* data, const segment_t* shmSeg, const char* reqFile, int segSize, int* rxFileSize ) ```
This processes the cached file transfer for the webproxy thread (SHM client). The data argument is a file handle for the current threads temporary file. The shmSeg argument is for the current SHM segment_t from the SHM pool. The reqFile argument
is the path of the requested file. The segSize argument is the size of the SHM segment. And, rxfileSize is the received cached file length computed by the SHM library. This function first sends a segment name IPC message, and waits for a reply.
Next, it sends an file-transfer request IPC message and waits for a reponse, from which it extracts the file length. It then sends an IPC message saying it is ready for the file transfer. It then enters a while() loop in which it waits for a transaction
IPC message, grabs the current number of bytes transfered, writes the data to the temporary file, and sends a reply saying it is ready for the next message. 

*** SHM Library SHM Pool Allocation ***

The below function was used for allocating a pool of SHM segments, and is called by webproxy main((). The segArray contains 3 elements, msgQInitVal, segName, and virtAddr.
The msgQInitVal is the initial message queue type name, corresponding to a unique segment name, to use for each cached file transaction. The virtAddr is just the 
virtual address corresponding to the segment name. The msgQInitVal prevents threads from retrieving messages intended for other threads; it aids in syncrhonization. 

```c
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
```

*** SHM Library Transaction Diagram - Shows the interaction between a single webproxy reqeust and the simplecached processing of that single request. ***

This project involved creating our own synchronization protocol for accessing the shared memory, and this was the most challenging part.
I created a UML-like diagram to describe all of the synchronization between the webproxy thread and a simplecached process for a single file request. It contains the diagram, a diagram key, and a brief description.
Please let me know if you can't access this diagram. 

* Diagram URL: ***  https://www.gliffy.com/go/share/si9hfdkyh1eitglce2s9 ***

*** Design Choices ***

** Regarding segment usage **

I had webproxy allocated a pool of SHM segment_t structs. The handle_with_cache function interacted with a queue of indexes into the SHM pool. It would pop a SHM pool index off of this queue, and then pass that to SHM_ClientTransfer. 
Once SHM_ClientTransfer completed, it would put the SHM pool index back on the queue. This queue was protected by a mutex. 
I chose to use a single SHM segment per transaction for simplicity. The alternative was to allow for several SHM segments per transaction. 

** Regarding handle_with_cache **

I chose to make handle_with_cache perform similar to handle_with_curl. By this, I mean handle_with_cache would first acquire the full file from the cache server, prior to sending it to the GFS client. The alternative was
to modify SHM_TransactClient so it would both acquire the file from the cache server, and send it to the GFS client simultaneously. 
This requires 2 function pointer arguments to SHM_TransactClient: 1) typedef ssize_t (*fileSendPtr)(gfcontext_t*, void*, size_t); 2) ssize_t (*headerSendPtr)(gfcontext_t*, gfstatus_t, size_t);. 
Here, fileSendPtr is a function pointer for gfs_send,and headerSendPter is a function pointer for gfs_sendheader.  I successfully implemented the SHM_TransactClient this way, but may or may not have this implemented in the final
checked-in version. On the test server, the alternative method (eliminating the temporary file), showed no noticable improvement on the class test server. 

** Regarding segments and mmap **

I chose to have a simplecached Boss thread map the current SHM segment to its virtual memory per file request. The alternative was to do this mapping at initialization time (prior to the Boss function starting). However, this would 
have involved increased complexity in order to find the virtual address corresponding to the current SHM segment from a list of SHM segments mapped to virtual addresses. This would have been simple if C++ was used with a map() structure, 
but seemed overly complex for this situation. 

** Regarding the use of a unique initial message queue ID for each thread to perform the file transfer **

I found that if I didn't sequence the message IDs within the cache file transfer while() loop, some messages would arrive out of order, leading to a corrupted file. To resolve this, I had each thread have a unique starting IPC message ID.
This unique ID was incremented on each iteration of the file transfer loop per thread. I also used the unique message queue ID for all IPC syncrhonization calls prior to the file transfer, except for the "segment name and message queue ID" message. 
This aided in syncrhonization by forcing each pair of threads to use a different message type. 

** Regarding the Choice of a Message Queue for IPC **

I chose to use a message queue for IPC instead of shared memory for IPC, an alternative I considered. I did this because I would have had to create a means of synchronizing the IPC SHM segment otherwise. The message queue was simpler. 
However, during the cahced file transfer, a pair of messages was sent for each file chunk transaction. This is expensive, and use of shared memory IPC would have probably caused a performance improvement. 
I used three message queues, one for the intial IPC communications prior to actually transfering the cached file, one for sending a file chunk, and one for sending a receipt message
for the file chunk. 

- __What created problems for you?__

I had several problems with this poject, primarily related to learning how to use SHM and IPC. Some of the problems I had are described in "Design Choices".
Here I list the problems I encountered, and the rememdy I used. 

1) Initially I had problems with passing around the SHM virtual address pointer, which was causing segmentation violations.

Remedy: I wasn't using a double pointer and should have been, so I changed it. 

2) File chunks were received out of order, during cached file transaction. 

Remedy: I created a unique starting message ID corresponding to each SHM segment. This ID was used for all IPC synchronization methods, except of course the first message which provides this ID.
This unique ID was incremented per each file chunk trasnfer iteration per thread. This allowed the SHM client and SHM server to agree upon IPC message IDs for every single file chunk transfer. 
To eliminate overlap, the unique IDs were separated by IPC_MSG_THREAD_FACT (10,000). So, thread0 would have unique ID 10,000, thread1 would have unique ID 20,000, thread2 would have 30,000, etc. 

3) Passing the "test proxy for multi-threading efficacy" test. 

Initially, I found that the following works: variable number of webproxy threads, variable number of simplecached threads, and only a single gfclient_download thread.
If I use multiple gfclient_download threads, the transaction fails frequently, but not always. The failure rate seems dependent on the number of gfclient_download threads.
For example, using 2 threads leads to a higher success rate than 8 threads. It took me a long time to reproduce this problem on my local machine. 

Evetually, I found that the failures happend when the number of segments was more than 1/2 the number of webproxy threads. For example, if I used "webproxy -t 8 -n 4", it would 
work repeatedly, whereas ("webproxy -t 8 -n 8" would fail. 

The symptom of the failure was that most of the files are transfered correctly. But, a couple of files get stuck waiting for a message (on the message queue) during the file transfer between webproxy
and simplecached. This makes it seem like somehow, messages (on message queue) are getting lost. I would see the SHM server send a message ID, say 10009, but the SHM client would
never receive it, showing the last message it received to be 10008. So, what happened to message ID 10009? 

Initally, other than using a single thread for webproxy, the only way I could get all of the tests to pass, except for the "test proxy for multi-threading efficacy" test, was to place a mutex around the webclient cache transaction function,
 SHM_TransactClient(). This is shown here:
 
```c 
Within handle_with_cache function
	//---- Read the file from cache
    pthread_mutex_lock(&wMutex);
	shmRes = SHM_TransactClient( tempFile, &curSegment, fullPath, segSize, &fileLen );
    pthread_mutex_unlock(&wMutex);	
```

This essentially serializes the webproxy threads. Interestingly, where I placed this mutext mattered. 

I tried a number of remedies for this problem, and only the last one resolved the problem:
1) Use a named semaphore for synchronizing the message queue msgrcv() and msgsnd().
2) Placing a mutex in some locations in SHM_TransactClient().
3) Using an unnamed semaphore for synching the threads in each respecitive process for the cached file transfer. 
4) Using a separate message queue for transfering file chunks and receiving a response for the file chunk. This method I actually implemented, and kept. 
5) Finally, I place mutex in SHM_TransactClient() around the transmission of message that sent the SHM segment name and message queue ID/type to the SHM server. This method fixed my problem.

The 5th rememdy listed above solved the problem discussed here. For the message that sent the segment name and message queue ID/type (for later IPC synchronization), the same message type was used for all 
of the webproxy threads, leading to syncrhonization issues. Placing a mutex around the transfer and receipt of this message caused each thread to only send its segment name, message queue ID/type, and receive the file size for its request,
one at a time. 

- __What tests would you have added to the test suite?__

Some tests I would have added are: 1) repeatedly trying to download a non-existent file. 2) Using a very small segment size (I actually used 16 bytes for this, and it still worked). I feel the test server provided a good range of tests. 
I also ran tests using large files, MP3s specifically, that were several MB large. This too worked fine. 

- __If you were going to do this project again, how would you improve it?__

Some improvements I could have done are:

1) Use a shared memory region for IPC during the cached-file transfer. This should imrpove performance over using a message queue, which is what I used, but would have added even more complexity. 
I'm not sure if the added complexity would be worth it. 

2) Eliminate the use of a temporary file in handle_with_cache for acquiring the cached file before sending it to the GFS Client. This is discussed above in the "Design Choices" section under "handle_with_cache". 
I would have passed function pointers to gfs_send and gfs_sendheader to SHM_TransactClient, so SHM_TransactClient could server the GFS Client while obtaining the cached file from the cache server. 
But, this would have made "SHM_TransactClient" not interchangable with "RunCurl", which would have made the extra credit portion involve two versions of SHM_TransactClient, one for the extra credit and one for part 2 of the project. 
This seemed silly. 

- __If you didn't think something was clear in the documentation, what would you write instead?__

Now, I think the documentation is clear. However, I had to read and think about its contents many times before I understood well enough to complete this project. This project is complex, and I don't think
the documentation could have been better and not give anything away. 


### Extra Credit: Integrate Project 1 with Project 3

- __Summary Description__

Part 3 involved taking my own gfserver and gflcient code (from Porject 1) and making it work with the webproxy, simplecached, and the CURL library. 
This builds a complete GETFILE-to-HTTP proxy server with cache. 

This involved modifying webrpoxy to use my gfserver implementation rather than the instructor-provided one. 
It involved no changes to simplecached, handle_with_cache, or shm_channel. 

I made a new gfserver handle function, called handle_with_all. This function required a modified argument, which is shown below.
Originally, in the handle_with_cache function, only "segSize" and "threadID" were used. And, in the handle_with_curl function, only "server" was used.
I modified the struct to also include the server URL, so that it contained all required info for both cache and CURL. 

typedef struct cacheArg
{
	int 	segSize;		// SHM segment size
	int 	threadID;	// ID of the thread
   char* server;		// server URL, to look for a non-local file
} cacheArg_t;

The callback handle, handle_with_all first checks for the file in cache, by calling SHM_TransactClient (just as was done in handle_with_cache). 
If this fails, it then concatenates the URL with the file path, and calls RunCurl (just as was done in handle with curl). From their, handle_with_all
proceeds in the same manner as both handle_with_cache and handle_with_curl.

To compile this project, just call "make all". I modified the makefile so it would build everything correctly. 

Then, open 3 terminals. In one termial, run ./webproxy, then in another run ./simplecached, and the order doesn't matter. 
Finally, in the 3rd one run ./gfclientdownload. 

Note that I used my original GF-server code (what we wrote was in the gflib portion of project 1), which does not have a "number of threads" option. 
I'm not sure if this was what was intended by the directions or not. 

- __What created problems for you?__

I didn't have any problems with this portion. The reason for this is because I abstracted both the CURL and cache file transactions to be a similar-looking
function call.

- __What tests would you have added to the test suite?__

My tests involved: 1) starting webproxy and simplecached in different order, 2) transfering a variety files, 3) Transfering file from cache 
4) Transfering files from URL, by making the cached-file transfer fail. 

- __If you were going to do this project again, how would you improve it?__

I am happy with how the extra credit portion turned out. 


## Known Bugs/Issues/Limitations

__Please tell us what you know doesn't work in this submission__

### Part 1: Create proxy server with libcurl. 

Everything works in Part 1. Both tests on the class test server and on my own system passed. 


### Part 2: Create a proxy server using shared-memory IPC. 

I had several problems with this poject, primarily related to learning how to use SHM and IPC. Some of the problems I had are described in "Design Choices".

However, after a ton of effort, I eventually got everything to work in Part 2. 

## References

### Part 1: Create proxy server with libcurl. 

I primarily used the libcurl website documentation, and the examples located here: https://curl.haxx.se/libcurl/c/example.html. 
I also made use of the libcurl example link provided in the project README file: https://www.hackthissite.org/articles/read/1078.

### Part 2: Create a proxy server using shared-memory IPC. 

I used serveral resources to learn about the POSIX SHM API and the System V message queue API. 

Here is a list of the primary resources, both of which provide excellent examples of SHM and IPC:
- http://beej.us/guide/bgipc/output/html/multipage/mq.html 
- http://www.cse.psu.edu/~deh25/cmpsc473/notes/OSC/Processes/shm.html
- https://guides.github.com/features/mastering-markdown/ (learning about how to use a markdown document properly)


















