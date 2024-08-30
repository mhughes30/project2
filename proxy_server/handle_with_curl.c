
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <curl/curl.h>
#include "gfserver.h"


#define BUFF_SIZE		4096
#define FILENAME_SIZE	64
#define ERROR_404		-5
#define CURL_SUCCESS	0
#define DO_VERBOSE		0


void 		InitCurl(void);
static int 	RunCurl( FILE* data, char* url );


//-------------------- handle_with_curl ---------------------//
ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg)
{
	size_t  fileLen 	= 0;
	size_t  bytesSent 	= 0;
	ssize_t readLen		= 0;
	ssize_t writeLen 	= 0;

	FILE* tempFile;
	char  tempFileName[FILENAME_SIZE];

	char dataBuff[BUFF_SIZE] = {0};
	char fullURL[BUFF_SIZE]  = {0};
	char *dataDir = arg;

	//printf("file: %s\n", path);
	//printf("thread: %lu\n", ctx->thread);

	sprintf(tempFileName, "%lu", ctx->thread);

	strcpy(fullURL, dataDir);
	strcat(fullURL, path);

	// open the temporary file
	tempFile = fopen(tempFileName, "wb+");

	//printf("URL: %s\n", fullURL);

	int curlRes = RunCurl(tempFile, fullURL );
	if (curlRes == ERROR_404)
	{
		fprintf(stderr, "404 - not found\n");
		return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);	
	}

	// Calculating the file size
	fseek(tempFile, 0, SEEK_END);
	fileLen = ftell(tempFile);
	fseek(tempFile, 0, SEEK_SET);

	//printf("fileLen: %lu\n", fileLen);

	// send header	
	gfs_sendheader(ctx, GF_OK, fileLen);

	// Sending the file contents chunk by chunk.
	bytesSent = 0;
	while (bytesSent < fileLen)
	{
		readLen = fread(dataBuff, 1, BUFF_SIZE, tempFile);
		if (readLen < 0)
		{
			fprintf(stderr, "handle_with_curl read error, %zd, %zu, %zu", readLen, bytesSent, fileLen );
			bytesSent = SERVER_FAILURE;
			break;
		}
		writeLen = gfs_send(ctx, dataBuff, readLen);
		if (writeLen != readLen)
		{
			fprintf(stderr, "handle_with_curl write error");
			bytesSent = SERVER_FAILURE;
			break;
		}
		bytesSent += writeLen;
	}

	fclose(tempFile);
	remove(tempFileName);	// physically remove the temporary file when done

	return bytesSent;
}


//-------------------- InitCurl ---------------------//
void InitCurl(void)
{
	// only called once per curl instance, for all threads
	// this must be called before any threads are created - not thread safe
	curl_global_init(CURL_GLOBAL_ALL);
}


//-------------------- handle_with_curl ---------------------//
static int RunCurl( FILE* data, char* url )
{
	CURL* 	 curlHandle;
	CURLcode result;
	int returnVal = CURL_SUCCESS;

	// Easy Curl initialization	
	curlHandle = curl_easy_init();

	// specify the options
	curl_easy_setopt(curlHandle, CURLOPT_URL, url);
	curl_easy_setopt(curlHandle, CURLOPT_HEADER, 0 );
	//curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, callback);
	curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, (void*)data);
	curl_easy_setopt(curlHandle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	curl_easy_setopt(curlHandle, CURLOPT_VERBOSE, DO_VERBOSE);
	// will fail on 404-not found, instead of returning the html page
	curl_easy_setopt(curlHandle, CURLOPT_FAILONERROR, 1);

	// obtain the data
	result = curl_easy_perform(curlHandle);	

	if (result != CURLE_OK)
	{
		returnVal = ERROR_404;
		fprintf( stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(result) );
	}

	// Curl Cleanup
	curl_easy_cleanup(curlHandle);
	curl_global_cleanup();
	
	return returnVal;
}






















