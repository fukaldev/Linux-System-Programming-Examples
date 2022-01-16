#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include "dataStructures.h"
#include "threadPool.h"
#include "printer.h"

int**           matrix; //Adjacency matrix of graph
int             max = -1; //Max numbered label for creating matrix
Pool            *threadPool;    //thread pool structure
int             startingThread; //initial pool size
pthread_t       resizerThread;  //resizer thread structure
int             maxThread;      //max # of thread pool thread number
DBEntry         **db;           //database for calculated paths
pthread_mutex_t quitMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dbMutex = PTHREAD_MUTEX_INITIALIZER;    //Database access mutex
pthread_cond_t  readCond = PTHREAD_COND_INITIALIZER;    //Database reader access condition var
pthread_cond_t  writeCond = PTHREAD_COND_INITIALIZER;   //Database writer access condition var
int             wr = 0, ar = 0, ww = 0, aw = 0;         //waiting reader, accessed reader, waiting writer, accesed writer number
int             socketFd;
int             quit = 0; //When this setted by signal, Threads stops and program ends, gracefuly
int             update = 0;     //Resizer is ongoing flag
int             willCreate = 0; //bookkeeping of created threads at resizer thread
int             created = 0;    //number of threads created after resizer created
int             filePathFd;
int             logPathFd;
int             pidFile;
int             waiting = 0;

void *workerThread(void *arg); //Thread function that will be executed by pool threads
void *resizer(void *arg);       //Resizer thread function
void termHandler(int signo);    //Signal handler
void cleanUp(void); //Clean function when the exit called
int quitFlag(void); //Checks an returns quit flag
void setFlag(void); //Sets quit flag
void cleaner(void); //Clean function at exit


int main(int argc, char *argv[])
{
        char            c;
        char*           filePath;
        int             iFlag = 0, pFlag = 0, oFlag = 0, sFlag = 0, xFlag = 0, err = 0, index = 0, status, compare;
        int             port;
        char*           logPath;
        char            readBuf[1024];//Reader buffer
        clock_t         elapsed;
        int             totalNode;
        int             totalEdge;
        int             counter = 0;
        int             comSocketFd;
        struct sockaddr_in server, client;
        pthread_t       resizerThread;
        time_t          t;
        pid_t           pid, sid;
        char            pidSTR[12];
        
        while((c = getopt(argc, argv, "i:p:o:s:x:")) != -1)
        {
                switch (c)
                {
                case 'i':
                        iFlag = 1;
                        filePath = optarg;
                        break;
                case 'p':
                        pFlag = 1;
                        port = strtol(optarg, NULL, 10);
                        break;
                case 'o':
                        oFlag = 1;
                        logPath = optarg;
                        break;
                case 's':
                        sFlag = 1;
                        startingThread = strtol(optarg, NULL, 10);
                        break;
                case 'x':
                        xFlag = 1;
                        maxThread = strtol(optarg, NULL, 10);
                        break;
                case '?':
			err = 1;
			break;
                }
        }

        if(iFlag == 0 || pFlag == 0 || oFlag == 0 || sFlag == 0 || xFlag == 0|| err)
        {
                print("USAGE: ./server -i [pathToFile] -p [PORT] -o[pathToLogFile] -s [initialThreadSize] -x [maxThreadSize]\n", STDERR_FILENO, 0);
                exit(EXIT_FAILURE);
        }

        if(startingThread > maxThread)
        {
                print("Starting thread number cannot be greater than max thread number: line: %d\n", STDERR_FILENO, 1, __LINE__);
                exit(EXIT_FAILURE); 
        }

        if(startingThread < 2)
        {
                print("Starting thread number must be greater than 2: line: %d\n", STDERR_FILENO, 1, __LINE__);
                exit(EXIT_FAILURE); 
        }

        pid = fork();
        
        if(pid < 0)
        {
                print("Error while forking: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        if(pid > 0)
        {
                exit(EXIT_SUCCESS);
        }

        umask(0);

        sid = setsid();
        if(sid < 0)
        {
                print("Error while setting sid: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE); 
        }

        close(STDIN_FILENO);

        pidFile = open("/tmp/server.pid", O_CREAT | O_EXCL | O_RDWR, 0666); //PID file will be kept on /tmp folder and deleted when process killed by signal.
        if(pidFile == -1)
        {
                print("Second server cannot be created, there is already a server running: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        filePathFd = open(filePath, O_RDONLY);
        if(filePathFd == -1)
        {
                remove("/tmp/server.pid");
                print("Cannot opened input file: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        sprintf(pidSTR, "%d", getpid());
        if(write(pidFile, &pidSTR, strlen(pidSTR)) == -1)
        {
                remove("/tmp/server.pid");
                print("Error while writing pid: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        if (open("/dev/null",O_RDONLY) == -1) 
        {
                print("Error while reopen stdin: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        
        logPathFd = open(logPath, O_WRONLY | O_TRUNC);
        if(logPathFd == -1)
        {
                remove("/tmp/server.pid");
                print("Cannot opened log file: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        dup2(logPathFd,STDOUT_FILENO);
        dup2(logPathFd,STDERR_FILENO);
        close(logPathFd);

        time(&t);
        print("Executing with parameters: ======%s", STDOUT_FILENO, 1, ctime(&t));
        time(&t);
        print("-i %s ======%s", STDOUT_FILENO, 2, filePath, ctime(&t));
        time(&t);
        print("-p %d ======%s", STDOUT_FILENO, 2, port, ctime(&t));
        time(&t);
        print("-o %s ======%s", STDOUT_FILENO, 2, logPath, ctime(&t));
        time(&t);
        print("-s %d ======%s", STDOUT_FILENO, 2, startingThread, ctime(&t));
        time(&t);
        print("-x %d ======%s", STDOUT_FILENO, 2, maxThread, ctime(&t));

        time(&t);

        struct sigaction sa;
        sigset_t set;

        sigemptyset(&set);
        sigaddset(&set, SIGINT);
        //Block SIGINT before setting handler
        if(sigprocmask(SIG_BLOCK, &set, NULL) == -1)
        {
                print("sigprocmask SIGINT: %s", STDERR_FILENO,1,strerror(errno));                
                _exit(EXIT_FAILURE);
        }

        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sa.sa_handler = termHandler;           //Set termHandler for SIGINT handler
        if(sigaction(SIGINT, &sa, NULL) == -1)
        {
                print("sigaction SIGINT handler: %s", STDERR_FILENO,1,strerror(errno));
                _exit(EXIT_FAILURE);
        }

        if(atexit(cleaner) == -1)
        {
                print("atexit error: %s", STDERR_FILENO,1,strerror(errno));
                _exit(EXIT_FAILURE);
        }

        print("Loading graph...======%s", STDOUT_FILENO, 1, ctime(&t));

        elapsed = clock();

        //First loop to learn total node size and edge size, also to learn max vertex label for creating adjacency matrix
        while(1)
        {
                status = read(filePathFd, &readBuf[index], 1);

                if(status == 0)
                        break;

                if(status == -1)
                {
                        print("Error while reading input file: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                        exit(EXIT_FAILURE);
                }

                if(readBuf[index] == '#' && index == 0)
                {
                        counter++;
                        while(readBuf[index] != '\n')
                        {
                                index++;
                                status = read(filePathFd, &readBuf[index], 1);
                                if(status == -1)
                                {
                                        print("Error while reading input file: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                        }
                        readBuf[index+1] = '\0';
                        if(counter == 3) //When we encounter with third #, we can take graph info
                        {
                                char numBuf[100];
                                int numIndex = 0;
                                int takeNum = 0;
                                
                                while(readBuf[numIndex] != ':')
                                        numIndex++;
                                
                                numIndex += 2;

                                while(readBuf[numIndex] != ' ')
                                        numBuf[takeNum++] = readBuf[numIndex++];

                                numBuf[takeNum] = '\0';
                                totalNode = strtol(numBuf, NULL, 10); //Total node count
                                
                                while(readBuf[numIndex] != ':')
                                        numIndex++;
                                numIndex += 2;
                                takeNum = 0;
                                
                                while(readBuf[numIndex] != ' ')
                                        numBuf[takeNum++] = readBuf[numIndex++];
                                
                                numBuf[takeNum] = '\0';
                                totalEdge = strtol(numBuf, NULL, 10);   //Total edge count
                        }
                        index = 0;
                        continue;
                }

                if(readBuf[index] == '\t')
                {
                        readBuf[index] = '\0';
                        compare = strtol(readBuf, NULL, 10);
                        if(compare > max) //If node label larger than compare, this is max label number for a vertex currently
                                max = compare;
                        index = 0;
                        continue;
                }

                if(readBuf[index] == '\n')
                {
                        readBuf[index] = '\0';
                        compare = strtol(readBuf, NULL, 10);
                        if(compare > max)       //If node label larger than compare, this is max label number for a vertex currently
                                max = compare;
                        index = 0;
                        continue;
                }

                index ++;

        }

        if(lseek(filePathFd, 0, SEEK_SET) == -1) //Turn back beginning of file
        {
                print("Error seek input file: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        matrix = (int**) malloc(sizeof(int*)*(max+1)); //Create dynamic adjacency matrix
        for(int i = 0; i < max+1; i++)
                matrix[i] = (int*)calloc((max+1), sizeof(int));
        
        int v1, v2;

        while(1)
        {
                status = read(filePathFd, &readBuf[index], 1);

                if(status == 0)
                        break;

                if(status == -1)
                {
                        print("Error while reading input file: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                        exit(EXIT_FAILURE);
                }

                if(readBuf[index] == '#' && index == 0)//Pass lines that starts with #
                {
                        while(readBuf[index] != '\n')
                        {
                                status = read(filePathFd, &readBuf[index], 1);
                                if(status == -1)
                                {
                                        print("Error while reading input file: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                        }
                        index = 0;
                        continue;
                }

                if(readBuf[index] == '\t') //Take first node as source
                {
                        readBuf[index] = '\0';
                        v1 = strtol(readBuf, NULL, 10);
                        index = 0;
                        continue;
                }

                if(readBuf[index] == '\n') //Take second node as destination and add edge to graph
                {
                        readBuf[index] = '\0';
                        v2 = strtol(readBuf, NULL, 10);
                        matrix[v1][v2] = 1;
                        index = 0;
                        continue;
                }

                index ++;

        }

        elapsed = clock() - elapsed;
        double timeTake = ((double)elapsed)/CLOCKS_PER_SEC;

        time(&t);
        print("Graph loaded in %f seconds with %d nodes and %d edges ======%s", STDOUT_FILENO, 4, timeTake, totalNode, totalEdge, ctime(&t));

        memset(&server, 0, sizeof server);
        server.sin_family = AF_INET;    //Create server socket structure
        server.sin_port = htons(port);
        server.sin_addr.s_addr = INADDR_ANY;

        if((socketFd = socket(AF_INET, SOCK_STREAM, 0)) == -1)//Create server socket
        {
                print("Error while open server socket: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        if((bind(socketFd, (struct sockaddr *)&server, sizeof server)) == -1)//Bind address to socket
        {
                print("Error while bind server socket: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        if(listen(socketFd, maxThread) == -1)//Start listening
        {
                print("Error while listen server socket: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        int size = sizeof(struct sockaddr_in);

        threadPool = (Pool*)malloc(sizeof(Pool)); //Create thread pool structure
        if(!threadPool)
        {
                print("Error while creating thread pool: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        createPool(threadPool, startingThread, maxThread, workerThread);//Initialize thread pool

        db = malloc(sizeof(DBEntry*)*(max+1));//Create database array for lists
        if(!db)
        {
                print("Error while creating database: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE); 
        }
        
        for(int i = 0; i < max+1; i++) //initialize database array
                db[i] = NULL;
        
        if((status = pthread_mutex_lock(&(threadPool->mutex)) != 0))
        {
                print("Error : pthread_mutex_lock %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE); 
        }

        while(startingThread != threadPool->ready)//wait at boot until all starting threads ready
        {
                if((status = pthread_cond_wait(&(threadPool->waitMain), &(threadPool->mutex))) != 0)
                {
                        print("Error : pthread_cond_wait %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                        exit(EXIT_FAILURE); 
                }
        }

        if((status = pthread_mutex_unlock(&(threadPool->mutex)) != 0))
        {
                print("Error : pthread_mutex_unlock %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE); 
        }

        if((status = pthread_create(&resizerThread, NULL, resizer, NULL)) != 0) //When all threads ready we can create resizer thread
        {
                print("Error : pthread_mutex_unlock %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE); 
        }

        //Now all data sturctures created, unblock SIGINT
        if(sigprocmask(SIG_UNBLOCK, &set, NULL) == -1)
        {
                print("sigprocmask SIGINT: %s", STDERR_FILENO,1,strerror(errno));                
                exit(EXIT_FAILURE);
        }

        while(!quitFlag())
        {
                comSocketFd = accept(socketFd, (struct sockaddr *)&client, (socklen_t*)&size); //accept incoming connections
                if(errno == EINTR)
                        break;
                if(comSocketFd == -1)
                {
                        print("Error while accept connection server socket: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                        exit(EXIT_FAILURE);
                }

                if((status = pthread_mutex_lock(&(threadPool->mutex)) != 0))
                {
                        print("Error : pthread_mutex_lock %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                        exit(EXIT_FAILURE); 
                }

                while(threadPool->ready == 0)
                {
                        time(&t);
                        print("No thread is available! Waiting for one. ======%s", STDOUT_FILENO, 1, ctime(&t));
                        waiting = 1;
                        if((status = pthread_cond_wait(&(threadPool->waitMax), &(threadPool->mutex))) != 0)
                        {
                                print("Error : pthread_cond_wait %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                }
                
                addRequest(threadPool, comSocketFd); //when a thread available, send request connection file descriptor to queue to take thread
                
                if((status = pthread_cond_signal(&(threadPool->cond))) != 0) //wake up one of the waiting threads 
                {
                        print("Error : pthread_cond_signal %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                        exit(EXIT_FAILURE);
                }       

                if((status = pthread_mutex_unlock(&(threadPool->mutex)) != 0))
                {
                        print("Error : pthread_mutex_unlock %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                        exit(EXIT_FAILURE); 
                }
        }

        if((status = pthread_mutex_lock(&(threadPool->mutex)) != 0))
        {
                print("Error : pthread_mutex_unlock %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE); 
        }

        while(threadPool->pendingQueue != 0)
        {
                if((status = pthread_cond_wait(&(threadPool->waitFinish), &(threadPool->mutex))) != 0)
                {
                        print("Error : pthread_cond_wait %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                        exit(EXIT_FAILURE);
                }
        }

        if((status = pthread_mutex_unlock(&(threadPool->mutex)) != 0))
        {
                print("Error : pthread_mutex_unlock %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE); 
        }        

        pthread_cond_broadcast(&(threadPool->cond));
        pthread_cond_signal(&(threadPool->resizerCond));

        for(int i = 0; i < threadPool->totalThreadNum; i++)
        {       
                pthread_join(threadPool->threads[i], NULL);
        }

        pthread_join(resizerThread, NULL);

        print("All threads have terminated, server shutting down.\n", STDOUT_FILENO, 0);

        exit(EXIT_SUCCESS);
}

void *workerThread(void *arg)
{
        int tid = *((int*)arg); //take thread argument and free it
        free(arg);
        int index, status, req, finish = 0;
        time_t t;
        int nodes[2];
        while(!quitFlag())
        {
                if((status = pthread_mutex_lock(&(threadPool->mutex)) != 0))
                {
                        print("Error : pthread_mutex_lock %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                        exit(EXIT_FAILURE); 
                }

                threadPool->ready += 1; //thread ready and waiting

                if(update == 1 && willCreate == created)
                {//Resizer waits for other threads to created, so wake up resizer when load less then 75%
                        update = 0;
                        created = 0;
                        willCreate = 0;
                        if((status = pthread_cond_signal(&(threadPool->resizerCond))) != 0)
                        {
                                print("Error : pthread_cond_signal %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                                exit(EXIT_FAILURE);  
                        }
                }

                if(update == 1)
                        created += 1;
                
                time(&t);
                print("Thread #%d: waiting for connection ======%s", STDOUT_FILENO, 2, tid, ctime(&t));
                
                /*************JUST FOR BOOT PROCESS******************/
                if(startingThread == threadPool->ready)
                {
                        if((status = pthread_cond_signal(&(threadPool->waitMain))) != 0) //If all init threads are ready wake up main thread
                        {
                                print("Error : pthread_cond_signal %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                                exit(EXIT_FAILURE); 
                        }
                }
                /***************************************************/
                if(threadPool->ready == 1 && waiting == 1) //If we reached max threds and ready thread num if previously 0, this mean main thread is waiting for so, we wake up main thread if it is waiting
                {
                        if((status = pthread_cond_signal(&(threadPool->waitMax))) != 0)
                        {
                                print("Error : pthread_cond_signal %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                                exit(EXIT_FAILURE); 
                        }
                        waiting = 0;
                }
                
                while((threadPool->pendingQueue) == 0)//If there is no request in queue, wait for request
                {
                        if((status = pthread_cond_wait(&(threadPool->cond), &(threadPool->mutex))) != 0)
                        {
                                print("Error : pthread_cond_wait %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                                exit(EXIT_FAILURE); 
                        }
                        if(threadPool->pendingQueue == 0 && quitFlag())
                        {
                                if((status = pthread_mutex_unlock(&(threadPool->mutex)) != 0))
                                {
                                        print("Error : pthread_mutex_unlock %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                                        exit(EXIT_FAILURE); 
                                }
                                finish = 1;
                                break;
                        }                        
                }

                if(finish == 1)
                        break;

                req = threadPool->requestQueue[threadPool->head]; //Take request socket fd
                threadPool->head = (threadPool->head + 1 ) % threadPool->queueSize; //move head
                (threadPool->pendingQueue)--;   //decrease pending requests
                
                if(quitFlag() && threadPool->pendingQueue == 0)//If quitFlag setted and there is no request in queue, wake up main thread to join all threads
                {
                        if((status = pthread_cond_signal(&(threadPool->waitFinish))) != 0)
                        {
                                print("Error : pthread_cond_signal %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                                exit(EXIT_FAILURE); 
                        }
                }        

                (threadPool->ready)--;          //Thread is going to be busy, decrease ready thread count
                time(&t);
                print("A connection has been delegated to thread id #%d, system load %.1f%% ======%s", STDOUT_FILENO, 3, tid, 100-(100*(((double)(threadPool->ready))/(threadPool->totalThreadNum))), ctime(&t));
                
                if(100-(100*(((double)(threadPool->ready))/(threadPool->totalThreadNum))) >= 75 && threadPool->totalThreadNum != maxThread)
                {       //IF load reaches 75% then we must wake up resizer thread
                        if((status = pthread_cond_signal(&(threadPool->resizerCond))) != 0)
                        {
                                print("Error : pthread_cond_signal %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                }
                
                if((status = pthread_mutex_unlock(&(threadPool->mutex)) != 0))
                {
                        print("Error : pthread_mutex_unlock %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                        exit(EXIT_FAILURE); 
                }

                int *path;

                if(read(req, &nodes[0], sizeof(int)*2) == -1)
                {
                        print("Error : reading request %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                        exit(EXIT_FAILURE); 
                }

                if(nodes[0] >= max || nodes[1] >= max)
                {
                        int size = 1;
                        int send = -1;
                        if((status = pthread_mutex_lock(&(threadPool->mutex)) != 0))
                        {
                                print("Error : pthread_mutex_lock %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                                exit(EXIT_FAILURE); 
                        }

                        time(&t);
                        print("Thread #%d: Nodes are not in graph, no path possible!!! ======%s", STDOUT_FILENO, 2, tid, ctime(&t));
                        
                        if((status = pthread_mutex_unlock(&(threadPool->mutex)) != 0))
                        {
                                print("Error : pthread_mutex_unlock %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                                exit(EXIT_FAILURE); 
                        }
                        
                        if(write(req, &size, sizeof(int)) == -1)
                        {
                                print("Error : write %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        
                        if(write(req, &send, sizeof(int)*size) == -1)
                        {
                                print("Error : write %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        if(close(req) == -1)
                        {
                                print("Error : close %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        continue;
                }

                DBEntry *entry;

                //READER
                if((status =pthread_mutex_lock(&(dbMutex))) != 0)
                {
                        print("Error : pthread_mutex_lock %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                        exit(EXIT_FAILURE);
                }
                while(aw+ww > 0) //If there is a writer writing or waiting for writing, reader must wait since priority of writer sis bigger
                {
                        wr += 1; //Increase waiting reader
                        if((status = pthread_cond_wait(&readCond, &dbMutex)) != 0)
                        {
                                print("Error : pthread_cond_wait %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        wr -= 1;
                }
                ar += 1; //Now a reader accessed to db
                if((status = pthread_mutex_unlock(&(dbMutex))) != 0)
                {
                        print("Error : pthread_mutex_unlock %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                        exit(EXIT_FAILURE);
                }

                //now read
                time(&t);
                print("Thread #%d: searching database for a path from node %d to node %d ======%s", STDOUT_FILENO, 4, tid, nodes[0], nodes[1], ctime(&t));
                entry = findDB(db, nodes[0], nodes[1]);
                
                if((status = pthread_mutex_lock(&(dbMutex))) != 0)
                {
                        print("Error : pthread_mutex_lock %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                        exit(EXIT_FAILURE);
                }

                ar -= 1; //Increase number of reader that currently reading since reading is done
                if(ar == 0 && ww > 0) //If this is the last reader and there is writers that waiting then wake up one of the writers
                {
                        if((status = pthread_cond_signal(&writeCond)) != 0)
                        {
                                print("Error : pthread_cond_signal %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                }
                
                if((status = pthread_mutex_unlock(&(dbMutex))) != 0)
                {
                        print("Error : pthread_mutex_unlock %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                        exit(EXIT_FAILURE);
                }
                
                index = 0;
                if(entry == NULL)//Writer will be here
                {
                        time(&t);
                        print("Thread #%d: no path in database, calculating %d->%d ======%s", STDOUT_FILENO, 4, tid, nodes[0], nodes[1], ctime(&t));
                        
                        BFS(nodes[0], nodes[1], max, matrix, &path);//Make BFS to find path inside graph and save it to given path array
                        if(path[index] != -1)
                        {
                                if((status = pthread_mutex_lock(&(dbMutex))) != 0)
                                {
                                        print("Error : pthread_mutex_lock %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                                        exit(EXIT_FAILURE);
                                }

                                while(aw+ar > 0) //If there is no reader currently reading, or no writer currently writing then access to db otherwise wait
                                {
                                        ww += 1;
                                        pthread_cond_wait(&writeCond, &dbMutex);
                                        ww -= 1;
                                }
                                aw += 1; //writer access to db
                                
                                if((status =pthread_mutex_unlock(&(dbMutex))) != 0)
                                {
                                        print("Error : pthread_mutex_unlock %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                                        exit(EXIT_FAILURE);
                                }

                                addDB(db, path); //Now we can write

                                if((status =pthread_mutex_lock(&(dbMutex))) != 0)
                                {
                                        print("Error : pthread_mutex_lock %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                                        exit(EXIT_FAILURE);
                                }

                                aw -= 1;        //writing done
                                if(ww > 0)      //If there is more waiting writers then wake up them
                                {
                                        if((status = pthread_cond_signal(&writeCond)) != 0)
                                        {
                                                print("Error : pthread_cond_signal %s line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }
                                }
                                else
                                {       //If there is no more writer then wake up all readers
                                        if((status = pthread_cond_broadcast(&readCond)) != 0)
                                        {
                                                print("Error : pthread_cond_broadcast %s line: %d\n", STDOUT_FILENO, 2, strerror(status), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }
                                }
                                
                                if((status =pthread_mutex_unlock(&(dbMutex))) != 0)
                                {
                                        print("Error : pthread_mutex_unlock %s line: %d\n", STDOUT_FILENO, 2, strerror(status), __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                                
                                if((status = pthread_mutex_lock(&(threadPool->mutex)) != 0))
                                {
                                        print("Error : pthread_mutex_lock %s line: %d\n", STDOUT_FILENO, 2, strerror(status), __LINE__);
                                        exit(EXIT_FAILURE); 
                                }
                                print("Thread #%d: path calculated: ", STDOUT_FILENO, 1, tid);
                        }
                        else
                        {
                                time(&t);
                                print("Thread #%d: path not possible from node %d to %d ======%s", STDOUT_FILENO, 4, tid, nodes[0], nodes[1], ctime(&t));
                        }
                }
                else
                {
                        if((status = pthread_mutex_lock(&(threadPool->mutex)) != 0))
                        {
                                print("Error : pthread_mutex_lock %s line: %d\n", STDOUT_FILENO, 2, strerror(status), __LINE__);
                                exit(EXIT_FAILURE); 
                        }
                        print("Thread #%d: path found in database: ", STDOUT_FILENO, 1, tid);
                        path = entry->path;        
                }
                
                index = 0;
                if(path[index] != -1)
                {        while(path[index] != -1)
                        {
                                print("%d", STDOUT_FILENO, 1,path[index]);
                                if(path[index+1] != -1)
                                        print("->", STDOUT_FILENO, 0);
                                index++;
                        }
                        time(&t);
                        print(" ======%s", STDOUT_FILENO, 1, ctime(&t));
                        if((status = pthread_mutex_unlock(&(threadPool->mutex)) != 0))
                        {
                                print("Error : pthread_mutex_lock %s line: %d\n", STDOUT_FILENO, 2, strerror(status), __LINE__);
                                exit(EXIT_FAILURE); 
                        }
                }

                if(entry == NULL)
                {
                        time(&t);
                        print("Thread #%d: responding to client and adding path to database ======%s", STDOUT_FILENO, 2, tid, ctime(&t));
                }

                index += 1;

                if(write(req, &index, sizeof(int)) == -1)//Send size of path
                {
                        print("Error : write %s line: %d\n", STDOUT_FILENO, 2, strerror(errno), __LINE__);
                        exit(EXIT_FAILURE);
                }
                
                if(write(req, path, sizeof(int)*index) == -1)//Send path
                {
                        print("Error : write %s line: %d\n", STDOUT_FILENO, 2, strerror(errno), __LINE__);
                        exit(EXIT_FAILURE);
                }

                if(path[0] == -1)//If path not putted on db, then free, so don't turn back to freeing
                        free(path);

                if(close(req) == -1)
                {
                        print("Error : close %s line: %d\n", STDOUT_FILENO, 2, strerror(errno), __LINE__);
                        exit(EXIT_FAILURE);
                }
                
        }
        return NULL;
}

void *resizer(void *arg)
{
        int status;
        while(!quitFlag())//Check quit flag is setted
        {
                if((status = pthread_mutex_lock(&(threadPool->mutex)) != 0))
                {
                        print("Error : pthread_mutex_lock %s line: %d\n", STDOUT_FILENO, 2, strerror(status), __LINE__);
                        exit(EXIT_FAILURE); 
                }

                while(100-(100*(((double)(threadPool->ready))/(threadPool->totalThreadNum))) < 75 || threadPool->totalThreadNum == maxThread)//if load is less than 75% or if we reach max thread number, wait
                {
                        if((status = pthread_cond_wait(&(threadPool->resizerCond), &(threadPool->mutex))) != 0)
                        {
                                print("Error : pthread_cond_wait %s line: %d\n", STDOUT_FILENO, 2, strerror(status), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        if(quitFlag())//If wake up occured by main thread, since quit flag setted break and below if line exit thread
                                break;
                }

                if(quitFlag())
                {
                        if((status = pthread_mutex_unlock(&(threadPool->mutex)) != 0))
                        {
                                print("Error : pthread_mutex_unlock %s line: %d\n", STDOUT_FILENO, 2, strerror(status), __LINE__);
                                exit(EXIT_FAILURE); 
                        }
                        break;
                }
                
                int newThreadNum = ((threadPool->totalThreadNum)*0.25); //Calculate newly created thread number

                while(newThreadNum + threadPool->totalThreadNum > maxThread)//If new thread number exceed max thread number, decrease it until they are equal
                        newThreadNum--;
                
                print("System load %.1f%%, pool extended to %d threads\n", STDOUT_FILENO, 2, 100-(100*(((double)(threadPool->ready))/(threadPool->totalThreadNum))), threadPool->totalThreadNum+newThreadNum);

                threadPool->threads = (pthread_t*)realloc(threadPool->threads, sizeof(pthread_t)*((int)((threadPool->totalThreadNum) + newThreadNum)));
                for(int i = threadPool->totalThreadNum; i < (threadPool->totalThreadNum)+newThreadNum; i++)//Realloc thread array in threadpool and create new threads
                {
                        int *id = malloc(sizeof(int));
                        *id = i;
                        pthread_create(&((threadPool->threads)[i]), NULL, workerThread, id);
                }
                (threadPool->totalThreadNum) += newThreadNum;
                willCreate = newThreadNum; //Keep number of threads to be created for this resizing session
                update = 1; //Set update to 1 to indicate updating thread numbers is ongoing
                while(update == 1 || threadPool->totalThreadNum == maxThread) //Until all newly created threads is ready wait and wakeup when all needed threads are created and ready
                {
                        if((status = pthread_cond_wait(&(threadPool->resizerCond), &(threadPool->mutex))) != 0)
                        {
                                print("Error : pthread_cond_wait %s line: %d\n", STDOUT_FILENO, 2, strerror(status), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        if(quitFlag())
                                break;
                }
                if((status = pthread_mutex_unlock(&(threadPool->mutex)) != 0))
                {
                        print("Error : pthread_mutex_unlock %s line: %d\n", STDOUT_FILENO, 2, strerror(status), __LINE__);
                        exit(EXIT_FAILURE); 
                }
        }
        return NULL;
}

//Start termination process
void termHandler(int signo)
{
        setFlag();
        if(close(socketFd) == -1)//Close socket since, server may stuck at accept call, this will wakeup and errno can be allowable
        {
                print("Error : while closing socket file %s line: %d\n", STDOUT_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        print("Termination signal received, waiting for ongoing threads to complete.\n", STDOUT_FILENO, 0);
}

//Return status of quit flag
int quitFlag()
{
        int flag;
        pthread_mutex_lock(&quitMutex);
        flag = quit;
        pthread_mutex_unlock(&quitMutex);

        return flag;
}

//Set quit flag
void setFlag()
{
        pthread_mutex_lock(&quitMutex);
        quit = 1;
        pthread_mutex_unlock(&quitMutex);
}

void cleaner(void)
{
        for(int i = 0; i < max+1; i++)
                free(matrix[i]);
        free(matrix);

        close(filePathFd);
        close(pidFile);

        free(threadPool->threads);
        free(threadPool->requestQueue);

        pthread_cond_destroy(&(threadPool->cond));
        pthread_cond_destroy(&(threadPool->resizerCond));
        pthread_cond_destroy(&(threadPool->waitFinish));
        pthread_cond_destroy(&(threadPool->waitMain));
        pthread_cond_destroy(&(threadPool->waitMax));

        pthread_mutex_destroy(&(threadPool->mutex));

        free(threadPool);

        DBEntry *temp;
        DBEntry *delete;
        for(int i = 0; i < max+1; i++)
        {
                if(db[i] != NULL)
                {
                        temp = db[i];
                        while(temp != NULL)
                        {
                                free(temp->path);
                                delete = temp;
                                temp = temp->next;
                                free(delete);
                        }
                }
        }
        free(db);
        remove("/tmp/server.pid");
}
