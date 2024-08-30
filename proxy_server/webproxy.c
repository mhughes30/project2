#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "gfserver.h"
                                                                \
#define USAGE                                                                 \
"usage:\n"                                                                    \
"  webproxy [options]\n"                                                     \
"options:\n"                                                                  \
"  -p [listen_port]    Listen port (Default: 8080)\n"                         \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1000)\n"      \
"  -s [server]         The server to connect to (Default: Udacity S3 instance)"\
"  -h                  Show this help message\n"                              \
"special options:\n"                                                          \
"  -d [drop_factor]    Drop connects if f*t pending requests (Default: 5).\n"


/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = 
{
  {"port",          required_argument,      NULL,           'p'},
  {"server",        required_argument,      NULL,           's'},         
  {"thread-count",  required_argument,      NULL,           't'},
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,             0}
};

extern ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg);
extern ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg);
extern void    InitCurl(void);

static gfserver_t gfs;

static unsigned short port 			 = 8080;
static unsigned short nworkerthreads = 1;
static char *server = "s3.amazonaws.com/content.udacity-data.com";



static void  ProcessCmdArgs(int argc, char* argv[]);
static void  _sig_handler(int signo);
static void  Usage();



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


	if (!server) 
	{
	   fprintf(stderr, "Invalid (null) server name\n");
	   exit(1);
  	}
  
  	/* SHM initialization...*/
	InitCurl();

  	/*Initializing server*/
  	gfserver_init(&gfs, nworkerthreads);

  	/*Setting options*/
  	gfserver_setopt(&gfs, GFS_PORT, port);
  	gfserver_setopt(&gfs, GFS_MAXNPENDING, 12);
  	gfserver_setopt(&gfs, GFS_WORKER_FUNC, handle_with_curl);
  
	for(i = 0; i < nworkerthreads; i++)
	{
	    gfserver_setopt(&gfs, GFS_WORKER_ARG, i, server);
	}

  	/*Loops forever*/
  	gfserver_serve(&gfs);
}


//--------------------- _sig_handler ---------------------//
static void _sig_handler(int signo)
{
	if (signo == SIGINT || signo == SIGTERM)
	{
		gfserver_stop(&gfs);
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
  	while ( (option = getopt_long(argc, argv, "p:s:t:h", gLongOptions, NULL)) != -1 ) 
	{
		switch (option) 
		{
	    case 'p': // listen-port
	        port = atoi(optarg);
	        break;
	    case 't': // thread-count
			nworkerthreads = atoi(optarg);
			break;
        case 's': // file-path
        	server = optarg;
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



