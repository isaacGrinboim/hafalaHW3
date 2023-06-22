#include <assert.h>
#include "segel.h"
#include "request.h"
#define NONUSED_ATTR NULL
// 
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
// 

// HW3: Parse the new arguments too
void getargs(int *port, int *numOfThreads, int *queueSize, char **overLoadHandlerAlg, int argc, char *argv[]);

void threadPoolInit(threadPool *threadyPool, int numOfThreads);

void *threadCodeToRun(void *arguments);

char* overloadHandlerAlg;

requestQueue queue;

pthread_cond_t fullQueue;

// pthread_cond_t notEmpty;

pthread_cond_t emptyQueue;

pthread_mutex_t lockQueue;

threadPool threadypool;

threadNode* myNode(pthread_t myself);

//int requestsInProgress = 0;

int main(int argc, char *argv[]) {
    overloadHandlerAlg = malloc((strlen(argv[4])+1) * sizeof(char));
    int listenfd, connfd, port, clientlen, numOfThreads, queueSize;
    //printf("before get args\n");
    getargs(&port, &numOfThreads, &queueSize, &overloadHandlerAlg, argc, argv);
    // printf("num threads: %d, queueSize: %d\n",numOfThreads, queueSize);
    struct timeval arrivalTime;

    pthread_mutex_init(&lockQueue,NONUSED_ATTR);

    // pthread_mutex_lock(&lockQueue);
    threadPoolInit(&threadypool, numOfThreads);
    // pthread_mutex_unlock(&lockQueue);

    //requestsInProgress = 0;
    InitRequestQueue(&queue, queueSize);
    int worked = 0;
    
    
    worked = pthread_cond_init(&fullQueue, NULL);
    if(worked!=0){
        //Todo: add error
    }
    worked = pthread_cond_init(&emptyQueue, NULL);
    
    if(worked!=0){
        //Todo:: add error
    }

    struct sockaddr_in clientaddr;
    // 
    // HW3: Create some threads...
    //
	
    listenfd = Open_listenfd(port);
    
    while (1) {
        clientlen = sizeof(clientaddr);
      //  printf("error here?\n");
      //  printf("try to get connfd\n");
        connfd = Accept(listenfd, (SA *) &clientaddr, (socklen_t *) &clientlen);
	//	printf("after get connfd\n");
        gettimeofday(&arrivalTime, NULL);

        worked = pthread_mutex_lock(&lockQueue);
        if(worked != 0){
            //Todo: add error;
        }
		//	printf("params: nums: %d _ %d _ %d\n", queue.numOfRequests ,requestsInProgress , queue.maxSize);
        if(queue.numOfRequests + queue.requestsInProgress  < queue.maxSize){
		//	printf("first time\n");
            pushRequestQueue(&queue, connfd, overloadHandlerAlg, &arrivalTime);

            pthread_cond_signal(&emptyQueue);
			printf("inserted to end of queue (no drop needed)\n");
            pthread_mutex_unlock(&lockQueue);
            continue;
        }
        if(strcmp(overloadHandlerAlg, "block") == 0){
			//printf("second time\n");
			//TODO: check if this logic can cause deadlock.
            while(queue.numOfRequests + queue.requestsInProgress >= queue.maxSize){
             //   printf("hello world %d")
                pthread_cond_wait(&fullQueue, &lockQueue);
            }
            if(queue.numOfRequests + queue.requestsInProgress < queue.maxSize)
            {
                pushRequestQueue(&queue,connfd,overloadHandlerAlg, &arrivalTime);
                pthread_cond_signal(&emptyQueue);
            }
            else {
                Close(connfd);
            }
            //printf("inserted to end of queue (drop required)\n");
            pthread_mutex_unlock(&lockQueue);
            continue;
        }
        else if(strcmp(overloadHandlerAlg, "dt") == 0){
            Close(connfd);
            pthread_mutex_unlock(&lockQueue);
            continue;
        }
        // else if(queue.numOfRequests == 0 || strcmp(overloadHandlerAlg, "dt") == 0 || (queue.maxSize == queue.dynamicMax)&& strcmp(overloadHandlerAlg, "dynamic")){
        //     Close(connfd);
        //     pthread_mutex_unlock(&lockQueue);
        //     continue;
        // }
        else if(strcmp(overloadHandlerAlg, "dh") == 0){
			// if(queue.numOfRequests == 0){
			// 	Close(connfd);
			// 	pthread_mutex_unlock(&lockQueue);
			// 	continue;
			// }
			// request* reqy = popRequestQueue(&queue);
            // Close(reqy->connfd);
            // //free(reqy);
            pushRequestQueue(&queue,connfd,overloadHandlerAlg, &arrivalTime);
            pthread_cond_signal(&emptyQueue);
            pthread_mutex_unlock(&lockQueue);
            continue;
        }
       else if(strcmp(overloadHandlerAlg, "bf") == 0){
           while(queue.numOfRequests + queue.requestsInProgress != 0){
            pthread_cond_wait(&fullQueue, &lockQueue);
           }
           Close(connfd);
           pthread_mutex_unlock(&lockQueue);
           continue;
       }
       else if(strcmp(overloadHandlerAlg, "dynamic") == 0){
            if(queue.maxSize == queue.dynamicMax){
                Close(connfd);
                pthread_mutex_unlock(&lockQueue);
                continue;
            }
            else{ 
                Close(connfd);
                queue.maxSize++;
                pthread_mutex_unlock(&lockQueue);
                continue;
            }
       }


// if random is implemented close the popped ones
        //
        // HW3: In general, don't handle the request in the main thread.
        // Save the relevant info in a buffer and have one of the worker threads
        // do the work.
        //
        //requestHandle(connfd);

       
    }//while parenthesis
    return 0;
}

void getargs(int *port, int *numOfThreads, int *queueSize, char **overLoadHandlerAlg, int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    if(argc == 6){
        queue.dynamicMax = atoi(argv[5]);
    }
    
    *port = atoi(argv[1]);
    *numOfThreads = atoi(argv[2]);
    *queueSize = atoi(argv[3]);
    strcpy(*overLoadHandlerAlg, argv[4]);
}

void threadPoolInit(threadPool *threadypool, int numOfThreads) {
    if (numOfThreads < 1) {
        app_error("invalid size of threads");
    }
   

    threadypool->threadRunning = 0;


    threadypool->threadsArr = NULL;
     
    threadypool->threadsArr = malloc(numOfThreads * sizeof(threadNode));
    
    if (threadypool->threadsArr == NULL) {
        unix_error("malloc failed");
    }
    threadypool->numOfThreads = numOfThreads;
    for (int i = 0; i < numOfThreads; ++i) {
        threadypool->threadsArr[i].numOfRequests = 0;
        threadypool->threadsArr[i].threadId = i;
        threadypool->threadsArr[i].totalRequestsHandled = 0;
        threadypool->threadsArr[i].staticRequestHandled = 0;
        threadypool->threadsArr[i].dynamicRequesrHandled = 0;
        threadypool->threadsArr[i].workingOn = NULL;
        int worked = 0;
        worked = pthread_create(&(threadypool->threadsArr[i].thready), NULL, &threadCodeToRun,
                                (void *) &(threadypool->threadsArr[i]));//Todo:implement threadCodeToRun
        if (worked != 0) {
            unix_error("failed to create thread");
        }
    }
}

void *threadCodeToRun(void *arguments) {
	threadNode* node = (threadNode*)arguments;
    int worked = 0;
    request* requestToWork;
    while (!0) {
        worked = pthread_mutex_lock(&lockQueue);
        if(worked != 0){
            //Todo: add error;
        }
        while(queue.numOfRequests == 0){
            worked = pthread_cond_wait(&emptyQueue,&lockQueue);
            if(worked!=0){
                //Todo: add error;
            }
        }
        
        requestToWork = popRequestQueue(&queue);
        struct timeval currentTime;
        gettimeofday(&currentTime, NULL);
        queue.requestsInProgress++;
       // pthread_cond_signal(&fullQueue);             _____________ I THINK IS NOT CORRECT
        worked = pthread_mutex_unlock(&lockQueue);
      //  printf("node: %d\n", node->thready);
       // printf("myself: %d\n", myNode(pthread_self())->thready);
        //printf("boolean value: %d\n", (node == myNode(pthread_self())));
        timersub(&currentTime, &(requestToWork->arrival), &(requestToWork->dispatch));
        node->workingOn = requestToWork;

        requestHandle(requestToWork->connfd, node);//myNode(pthread_self()));
        
        
		Close(requestToWork->connfd);//change made

		// free(requestToWork); //XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
		
		pthread_mutex_lock(&lockQueue);
		//printf("i am thready i subtract now from requestsInProgress\n");
        queue.requestsInProgress--;
        printf("finished handling request\n");
        // if((queue.numOfRequests + queue.requestsInProgress == 0) && (strcmp(overloadHandlerAlg, "bf") == 0)){
        //     pthread_cond_signal(&notEmpty);
        // }
        // if(strcmp(overloadHandlerAlg, "block") == 0 && queue.numOfRequests + requestsInProgress == (queue.maxSize - 1)){
        pthread_cond_signal(&fullQueue);
		// }
        
        pthread_mutex_unlock(&lockQueue);

        
		//printf("afetr free\n");
       
        if(worked!=0){
            //Todo: add error;
        }
    }
    return NULL;
}

threadNode* myNode(pthread_t myself){
    for(int i=0; i<threadypool.numOfThreads; ++i){
        if(threadypool.threadsArr[i].thready == myself){
            return &threadypool.threadsArr[i];
        }
    }
    return NULL;
}



