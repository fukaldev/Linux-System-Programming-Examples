#include "threadPool.h"

void createPool(Pool *threadPool, int startingThread, int queueSize, void* (*threadFunc)(void*))
{
        int status;
        threadPool->queueSize = queueSize; //Initialize queue size for requests
        threadPool->totalThreadNum = startingThread; 

        threadPool->tail = 0;//Tail of queue
        threadPool->head = 0;//head of queue
        threadPool->ready = 0;//number of ready thread
        threadPool->pendingQueue = 0;//Requests that pending at queue

        threadPool->threads = (pthread_t*) malloc(sizeof(pthread_t)*startingThread);
        if(!(threadPool->threads))
        {
                print("threadPool->threads malloc error in createPool: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        threadPool->requestQueue = (int*) malloc(sizeof(int)*queueSize);
        if(!(threadPool->requestQueue))
        {
                print("threadPool->requestQueue malloc error in createPool: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        if((status = pthread_cond_init(&(threadPool->cond), NULL)) != 0)
        {
                print("pthread_cond_init error in createPool: %s, line: %d", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE);
        }
        if((status = pthread_cond_init(&(threadPool->waitMain), NULL)) != 0)
        {
                print("pthread_cond_init error in createPool: %s, line: %d", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE);
        }
        if((status = pthread_cond_init(&(threadPool->resizerCond), NULL)) != 0)
        {
                print("pthread_cond_init error in createPool: %s, line: %d", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE);
        }
        if((status = pthread_cond_init(&(threadPool->waitMax), NULL)) != 0)
        {
                print("pthread_cond_init error in createPool: %s, line: %d", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE);
        }

        if((status = pthread_cond_init(&(threadPool->waitFinish), NULL)) != 0)
        {
                print("pthread_cond_init error in createPool: %s, line: %d", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE);
        }
        
        if((status = pthread_mutex_init(&(threadPool->mutex), NULL)) != 0)
        {
                print("pthread_mutex_init error in createPool: %s, line: %d", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE);
        }

        for(int i = 0; i  < startingThread; i++)
        {
                int *id = malloc(sizeof(int));
                *id = i;
                status = pthread_create(&((threadPool->threads)[i]), NULL, threadFunc, id);
                if(status != 0)
                {
                        print("pthread_create error in createPool: %s, line: %d", STDERR_FILENO, 2, strerror(status), __LINE__);
                        exit(EXIT_FAILURE);
                }
                
        }
}

void addRequest(Pool *threadPool, int req)
{
        threadPool->requestQueue[threadPool->tail] = req;
        threadPool->tail = (threadPool->tail + 1) % threadPool->queueSize;
        threadPool->pendingQueue += 1;
}