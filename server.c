#include <assert.h>
#include "segel.h"
#include "request.h"

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

void test(char* schedAlg,requestQueue* queue){
    for(int i = 500; i < 510; ++i){
        pushRequestQueue(&queue,i,schedAlg);
    }

    requestNode * check = queue->first;
    int i = 500;
    while(check){
        assert(check->req->connfd == i);
        check = check->next;
        ++i;
    }
    assert(i == 509);
}
void testPop(requestQueue* queue){
    for(int i = 500; i < 510; ++i){
        popRequestQueue(queue);
        assert(queue->first->req->connfd == i);
    }
    assert(queue->numOfRequests == 0);
}
int main(int argc, char *argv[]) {
    char *overloadHandlerAlg = NULL;
    int listenfd, connfd, port, clientlen, numOfThreads, queueSize;
    getargs(&port, &numOfThreads, &queueSize, overloadHandlerAlg, argc, argv);
    requestQueue* queue = NULL;
    InitRequestQueue(queue,10);
    test(overloadHandlerAlg,queue);
    testPop(queue);
    threadPool *threadypool = NULL;
    threadPoolInit(threadypool, numOfThreads);

    struct sockaddr_in clientaddr;



    // 
    // HW3: Create some threads...
    //

    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *) &clientaddr, (socklen_t *) &clientlen);

        //
        // HW3: In general, don't handle the request in the main thread.
        // Save the relevant info in a buffer and have one of the worker threads
        // do the work.
        //
        requestHandle(connfd);

        Close(connfd);
    }
}

void getargs(int *port, int *numOfThreads, int *queueSize, char *overLoadHandlerAlg, int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
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

    while (!0) {
        //pthread_mutex_lock(&) - lock queue;
    }
}



    


 
