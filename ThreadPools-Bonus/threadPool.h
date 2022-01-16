#ifndef POOL_H
#define POOL_H
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "printer.h"

typedef struct
{
        pthread_t       *threads;
        int             *requestQueue;
        pthread_cond_t  cond;
        pthread_cond_t  waitMain;
        pthread_cond_t  resizerCond;
        pthread_cond_t  waitFinish;
        pthread_cond_t  waitMax;
        pthread_mutex_t mutex;
        int             queueSize; //Total request queue size
        int             ready;   //ready thread number
        int             pendingQueue;  //Pending request at queue
        int             head;           //Head index of queue
        int             tail;           //Tail index of queue
        int             startedThread;
        int             totalThreadNum;
} Pool;

void createPool(Pool *threadPool, int startingThread, int queueSize, void*(*threadFunc)(void *arg));
void addRequest(Pool *threadPool, int req);

#endif