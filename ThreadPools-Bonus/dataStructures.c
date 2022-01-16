#include "dataStructures.h"

void createQueue(int capacity, Queue *queue)
{
        queue->front = 0; //initialize capacity, fron and rear index
        queue->rear = 0;
        queue->capacity = capacity;
        queue->q = (int*) malloc(sizeof(int)*capacity);
        if(!(queue->q))
        {
                print("queue->q malloc error in dataStructures: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        return;
}

void enqueue(Queue *queue, int item)
{
        if(queue->capacity == queue->rear) //Check queue is filled or not, if it is filled then add 10 more room
        {
                queue->q = realloc(queue->q, sizeof(int)*(queue->capacity+10));
                if(!(queue->q))
                {
                        print("queue->q realloc error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                        exit(EXIT_FAILURE);
                }
                queue->capacity += 10;
        }
        queue->q[(queue->rear)++] = item;
        return;
}

int dequeue(Queue *queue)
{       //Take item from queue and move head one more
        int frontItem = (queue->q)[0];
        for(int i = 0; i < (queue->rear)-1; i++)
                (queue->q)[i] = (queue->q)[i+1];
        queue->rear -= 1;
        return frontItem;
}

int isEmpty(Queue *queue)
{
        return queue->front == queue->rear ? 1 : 0;
}

void backTrace(int* parent, int start, int end, int max, int **realPath)
{       //Backtrace starting from destination node to end node
        int *path = (int*)malloc(sizeof(int)*(max+1));
        if(!path)
        {
                print("path malloc error in dataStructures: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);   
        }
        int totalPathNode = 1;
        int pathIndex = 0;
        path[pathIndex++] = end;
        while(path[pathIndex-1] != start)
        {
                path[pathIndex] = parent[path[pathIndex-1]];
                pathIndex++;
                totalPathNode += 1;
        }
        (*realPath) =(int*) malloc(sizeof(int)*(totalPathNode+1));
        if(!(*realPath))
        {
                print("realPath malloc error in dataStructures: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        for(int i = totalPathNode - 1; i > -1; i--)
                (*realPath)[totalPathNode - 1-i] = path[i];
        (*realPath)[totalPathNode] = -1;
        free(path);
        return;
}

void BFS(int startNode, int endNode, int max, int **matrix, int **path)
{
        Queue queue;
        createQueue(10, &queue);
        int *parent = malloc(sizeof(int)*(max+1));
        if(!parent)
        {
                print("parent malloc error in dataStructures: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        int node;
        int *visited = calloc(max+1, sizeof(int));

        enqueue(&queue, startNode);

        visited[startNode] = 1;

        while(!isEmpty(&queue))
        {       //Regular BFS but when current node is destination node, start backtracing and find path...
                node = dequeue(&queue);
                if(node == endNode)
                {
                        backTrace(parent, startNode, endNode, max, path);
                        free(parent);
                        free(visited);
                        free(queue.q);
                        return;
                }

                for(int i = 0; i < max+1; i++)
                {
                        if(matrix[node][i] == 1 && !(visited[i]))
                        {
                                parent[i] = node;
                                enqueue(&queue, i);
                                visited[i] = 1;
                        }
                }
        }
        (*path) = (int*)malloc(sizeof(int)*1); //If path is not available then create array but put -1 to first node to indicate no path available
        (*path)[0] = -1;
        free(parent);
        free(visited);
        free(queue.q);
        return;
}

DBEntry *findDB(DBEntry **db, int source, int destination)
{
        DBEntry *head = db[source]; //Take head pointer of bucket list head
        while(head != NULL) //Move until find destination point, if no path finded in db then return NULL
        {
                if(head->destination == destination)
                        return head;
                head = head->next;
        }

        return NULL;
}

void addDB(DBEntry **db, int *path)
{
        int source = path[0];
        int index = 0;

        DBEntry *temp = (DBEntry*) malloc(sizeof(DBEntry)); //Create new entry
        if(!temp)
        {
                print("temp malloc error in dataStructures: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        temp->next = NULL;

        while(path[index] != -1)
                index++;
        
        temp->path = path;
        temp->destination = path[index-1];
        DBEntry *head = db[source];

        if(head == NULL) //If head is NULL, put entry to head of list
        {
                db[source] = temp;
                return;
        }

        db[source] = temp;
        temp->next = head;
        
        return;
}