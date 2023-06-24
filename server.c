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

char *overloadHandlerAlg;

requestQueue queue;

pthread_mutex_t lockQueue;

pthread_cond_t fullQueue;

pthread_cond_t emptyQueue;

pthread_cond_t notEmpty;

threadPool threadypool;

int main(int argc, char *argv[]) {
    overloadHandlerAlg = malloc((strlen(argv[4]) + 1) * sizeof(char));
    int listenfd, connfd, port, clientlen, numOfThreads, queueSize;
    getargs(&port, &numOfThreads, &queueSize, &overloadHandlerAlg, argc, argv);
    InitRequestQueue(&queue, queueSize);
    struct timeval arrivalTime;

    pthread_mutex_init(&lockQueue, NONUSED_ATTR);
    
    
    int worked = 0;

    worked = pthread_cond_init(&fullQueue, NULL);
    if (worked != 0) {
        //Todo: add error
    }
    worked = pthread_cond_init(&emptyQueue, NULL);

    if (worked != 0) {
        //Todo:: add error
    }
    worked = pthread_cond_init(&notEmpty, NULL);
    struct sockaddr_in clientaddr;

	threadPoolInit(&threadypool, numOfThreads);
    listenfd = Open_listenfd(port);

    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *) &clientaddr, (socklen_t *) &clientlen);
        gettimeofday(&arrivalTime, NULL);

        worked = pthread_mutex_lock(&lockQueue);
        if (worked != 0) {
            //Todo: add error;
        }
        if (queue.numOfRequests + queue.requestsInProgress < queue.maxSize) {
            pushRequestQueue(&queue, connfd, overloadHandlerAlg, &arrivalTime);

            pthread_cond_signal(&emptyQueue);
            pthread_mutex_unlock(&lockQueue);
            continue;
        }
        if (strcmp(overloadHandlerAlg, "block") == 0) {
            while (queue.numOfRequests + queue.requestsInProgress  >= queue.maxSize) {
                pthread_cond_wait(&fullQueue, &lockQueue);
            }
            
            pushRequestQueue(&queue, connfd, overloadHandlerAlg, &arrivalTime);

            pthread_cond_signal(&emptyQueue);
            pthread_mutex_unlock(&lockQueue);
            continue;
        } else if (strcmp(overloadHandlerAlg, "dt") == 0) {
			pthread_mutex_unlock(&lockQueue);
            Close(connfd);
            
            continue;
        } else if (strcmp(overloadHandlerAlg, "dh") == 0) {
			if(queue.numOfRequests == 0){
			  
			  pthread_mutex_unlock(&lockQueue);
			  Close(connfd);
              continue;
			}
			request *req=popRequestQueue(&queue);
			Close(req->connfd);
			free(req);
            pushRequestQueue(&queue, connfd, overloadHandlerAlg, &arrivalTime);
            pthread_cond_signal(&emptyQueue);
            pthread_mutex_unlock(&lockQueue);
            continue;
        } else if (strcmp(overloadHandlerAlg, "bf") == 0) {
            while (queue.numOfRequests + queue.requestsInProgress != 0) {
                pthread_cond_wait(&notEmpty, &lockQueue);
            }
            pthread_mutex_unlock(&lockQueue);
            Close(connfd);
           
            continue;
        } else if (strcmp(overloadHandlerAlg, "dynamic") == 0) {
            if (queue.maxSize == queue.dynamicMax) {
				pthread_mutex_unlock(&lockQueue);
                Close(connfd);
                
                continue;
            } else {
				pthread_mutex_unlock(&lockQueue);
                Close(connfd);
                queue.maxSize++;
                
                continue;
            }
        }
        else if(queue.requestsInProgress == queue.maxSize){
			pthread_mutex_unlock(&lockQueue);
			Close(connfd);
            queue.maxSize++;
            
            continue;
		}
    }
}

void getargs(int *port, int *numOfThreads, int *queueSize, char **overLoadHandlerAlg, int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    if (argc == 6) {
        queue.dynamicMax = atoi(argv[5]);
    }

    *port = atoi(argv[1]);
    *numOfThreads = atoi(argv[2]);
    *queueSize = atoi(argv[3]);
    strcpy(*overLoadHandlerAlg, argv[4]);
}

void threadPoolInit(threadPool *threadypoolP, int numOfThreads) {
    if (numOfThreads < 1) {
        app_error("invalid size of threads");
    }
    threadypoolP->threadRunning = 0;
    threadypoolP->threadsArr = NULL;
    threadypoolP->threadsArr = malloc(numOfThreads * sizeof(threadNode));
    if (threadypoolP->threadsArr == NULL) {
        unix_error("malloc failed");
    }
    threadypoolP->numOfThreads = numOfThreads;
    for (int i = 0; i < numOfThreads; ++i) {
        threadypoolP->threadsArr[i].threadId = i;
        threadypoolP->threadsArr[i].totalRequestsHandled = 0;
        threadypoolP->threadsArr[i].staticRequestHandled = 0;
        threadypoolP->threadsArr[i].dynamicRequesrHandled = 0;
        threadypoolP->threadsArr[i].workingOn = NULL;
        int worked = 0;
        worked = pthread_create(&(threadypoolP->threadsArr[i].thready), NULL, threadCodeToRun,
                                (void *) &(threadypoolP->threadsArr[i]));//Todo:implement threadCodeToRun
        if (worked != 0) {
            unix_error("failed to create thread");
        }
    }
}

void *threadCodeToRun(void *arguments) {
    threadNode *node = (threadNode *) arguments;
    int worked = 0;
    request *requestToWork;
    while (1) {
        worked = pthread_mutex_lock(&lockQueue);
        if (worked != 0) {
            //Todo: add error;
        }
        while (queue.numOfRequests == 0) {
            worked = pthread_cond_wait(&emptyQueue, &lockQueue);
            if (worked != 0) {
//                //Todo: add error;
            }
        }

        requestToWork = popRequestQueue(&queue);
        struct timeval currentTime;
        gettimeofday(&currentTime, NULL);
        queue.requestsInProgress++;
        worked = pthread_mutex_unlock(&lockQueue);
        timersub(&currentTime, &(requestToWork->arrival), &(requestToWork->dispatch));
        node->workingOn = requestToWork;

        requestHandle(requestToWork->connfd, node);

        Close(requestToWork->connfd);//change made

        free(requestToWork);

        pthread_mutex_lock(&lockQueue);
        (queue.requestsInProgress)--;
        if(queue.numOfRequests + queue.requestsInProgress == 0){
            pthread_cond_signal(&notEmpty);
        }
        else{
            pthread_cond_signal(&fullQueue);
        }
        pthread_mutex_unlock(&lockQueue);

        if (worked != 0) {
            //Todo: add error;
        }
    }
    return NULL;
}



