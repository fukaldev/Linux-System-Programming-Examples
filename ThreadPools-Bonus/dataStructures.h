#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include "printer.h"

typedef struct
{
        int front;
        int rear;
        int capacity;
        int* q;
} Queue;

struct dbentry
{
        int destination;
        int *path;
        struct dbentry *next;
};

typedef struct dbentry DBEntry;

void createQueue(int capacity, Queue *queue);
void enqueue(Queue *queue, int item);
int dequeue(Queue *queue);
void printQueue(Queue *queue);
void backTrace(int* parent, int start, int end, int max, int **realPath);

void BFS(int startNode, int endNode, int max, int **matrix, int **path);

void createDB(DBEntry ***db);
void addDB(DBEntry **db, int *path);
DBEntry *findDB(DBEntry **db, int source, int destination);

#endif