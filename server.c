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
void getargs(int *port, int *numOfThreads, int *queueSize, char *overLoadHandlerAlg, int argc, char *argv[]);

void threadPoolInit(threadPool *threadyPool, int numOfThreads);

void *threadCodeToRun(void *arguments);

requestQueue queue;

pthread_cond_t fullQueue;

pthread_cond_t notEmpty;

pthread_cond_t emptyQueue;

pthread_mutex_t lockQueue;

int main(int argc, char *argv[]) {
    char *overloadHandlerAlg = NULL;
    int listenfd, connfd, port, clientlen, numOfThreads, queueSize;
    getargs(&port, &numOfThreads, &queueSize, overloadHandlerAlg, argc, argv);
    printf("argc count is: %d\n", argc);
   
    threadPool threadypool;
    
    
    
    threadPoolInit(&threadypool, numOfThreads);
    InitRequestQueue(&queue, queueSize);
    int worked = 0;
    worked = pthread_cond_init(&fullQueue, NONUSED_ATTR);
    if(worked!=0){
        //Todo: add error
    }
    worked = pthread_cond_init(&emptyQueue, NONUSED_ATTR);
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
        connfd = Accept(listenfd, (SA *) &clientaddr, (socklen_t *) &clientlen);
        worked = pthread_mutex_lock(&lockQueue);
        if(worked != 0){
            //Todo: add error;
        }
        if(queue.numOfRequests < queue.maxSize){
            pushRequestQueue(&queue, connfd, overloadHandlerAlg);
            pthread_cond_signal(&emptyQueue);
            pthread_mutex_unlock(&lockQueue);
            continue;
        }
        if(strcmp(overloadHandlerAlg, "block") == 0){
            while(queue.numOfRequests == queue.maxSize){
                pthread_cond_wait(&fullQueue, &lockQueue);
            }
        }
        else if(strcmp(overloadHandlerAlg, "dt") == 0 || (queue.maxSize == queue.dynamicMax)&& strcmp(overloadHandlerAlg, "dynamic")){
            Close(connfd);
            pthread_mutex_unlock(&lockQueue);
            continue;
        }
        else if(strcmp(overloadHandlerAlg, "dh") == 0){
            popRequestQueue(&queue);
        }
       else if(strcmp(overloadHandlerAlg, "bf") == 0){
           while(queue.numOfRequests != 0){
               pthread_cond_wait(&notEmpty, &lockQueue);
           }
       }
       else if(strcmp(overloadHandlerAlg, "dynamic") == 0){
            Close(connfd);
            queue.maxSize++;
            pthread_mutex_unlock(&lockQueue);
            continue;
       }
        pushRequestQueue(&queue,connfd,overloadHandlerAlg);
        pthread_cond_signal(&emptyQueue);
        pthread_mutex_unlock(&lockQueue);




        //
        // HW3: In general, don't handle the request in the main thread.
        // Save the relevant info in a buffer and have one of the worker threads
        // do the work.
        //
        //requestHandle(connfd);

        Close(connfd);
    }

}

void getargs(int *port, int *numOfThreads, int *queueSize, char *overLoadHandlerAlg, int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    if(argc == 6){
        queue.dynamicMax = argv[5];
    }
    *port = atoi(argv[1]);
    *numOfThreads = atoi(argv[2]);
    *queueSize = atoi(argv[3]);
    overLoadHandlerAlg = argv[4];

}

void threadPoolInit(threadPool *threadypool, int numOfThreads) {
    if (numOfThreads < 1) {
        app_error("invalid size of threads");
    }
    printf("error here?\n");

    threadypool->threadRunning = 0;
	printf("after\n");

    threadypool->threadsArr = NULL;
    threadypool->threadsArr = malloc(numOfThreads * sizeof(threadNode));
    if (threadypool->threadsArr == NULL) {
        unix_error("malloc failed");
    }
    threadypool->numOfThreads = numOfThreads;
    for (int i = 0; i < numOfThreads; ++i) {
        threadypool->threadsArr[i].numOfRequests = 0;
        threadypool->threadsArr[i].threadId = i;
        int worked = 0;
        worked = pthread_create(&(threadypool->threadsArr[i].thready), NULL, &threadCodeToRun,
                                (void *) &(threadypool->threadsArr[i].thready));//Todo:implement threadCodeToRun
        if (worked != 0) {
            unix_error("failed to create thread");
        }
    }
}

void *threadCodeToRun(void *arguments) {
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
        if(queue.numOfRequests == 0){
            pthread_cond_signal(&notEmpty);
        }
        pthread_cond_signal(&fullQueue);
        requestHandle(requestToWork->connfd);
        worked = pthread_mutex_unlock(&lockQueue);
        if(worked!=0){
            //Todo: add error;
        }
        free(requestToWork);
    }
    return NULL;
}





    


 
