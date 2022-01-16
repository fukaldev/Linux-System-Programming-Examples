#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <time.h>

#include "functions.h"

florist *floristArr = NULL; //Contains florist structures
customer *totalRequest = NULL; //Alll request will be stored here to free at the end
int totalRequestNum = 0; //All request num
int totalRequestSize = 5; //Initial request size;
customer **customerPerFlorist = NULL; //Queue of each florist stored in there
int *requestPerFlorist = NULL; //request number of each florist
int *queueSize = NULL; //store size of queue to enlarge when filled
pthread_t *floristThreads = NULL; //Thread structure array to hold threads
pthread_mutex_t mutex; //Main mutex to access shared data
pthread_mutex_t quitMutex; //Limit access to the quit flag
pthread_cond_t *condition = NULL; //Stores condition variable for each florist
florist **returnF = NULL; //Stores return value of each thread when they join
int floristNum = 0; //Total florist num
int quit = 0; //When this setted by signal, Threads stops and program ends, gracefuly
customer poison; //This is poison customer, when this put queue of a florist, if florist process it, he or she dies

int inputFd = -1;

void *floristFunc(void *arg); //Florist thread function
void cleanUp(void); //Clean function when the exit called
int quitFlag(void); //Checks an returns quit flag
void setFlag(void); //Sets quit flag


void termHandler(int signo); //Signal handler that sets quit flag

int main(int argc, char *argv[])
{
        char *input, c;
        int status, iFlag = 0, err = 0, index = 0;
        char lineBuffer[1024];

        srand(time(0));

        /************TAKE ARGUMENTS***********************/
        while((c = getopt(argc, argv, ":i:")) != -1)
        {
                switch (c)
                {
                case 'i':
                        iFlag = 1;
                        input = optarg;
                        break;
                case '?':
			err = 1;
			break;
                }
        }

        if(iFlag == 0)
        {
                print("File name is missing...\n", STDERR_FILENO, 0);
                print("USAGE: ./floristApp -i [fileName]\n", STDERR_FILENO, 0);
                _exit(EXIT_FAILURE);
        }
        else if (err)
        {
                print("USAGE: ./floristApp -i [fileName]\n", STDERR_FILENO, 0);
                _exit(EXIT_FAILURE);
        }
        /************************************************/

        //Register cleanUp to clean resources when program exit
        if(atexit(cleanUp) == -1)
        {
                print("Error while register atexit: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                _exit(EXIT_FAILURE);
        }

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

        if((inputFd = open(input, O_RDONLY)) == -1)//Open input file
        {
                print("Error while opening inputFile: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                _exit(EXIT_FAILURE);
        }

        print("Florist application initializing from file: %s\n", STDOUT_FILENO, 1, input);

        florist f;

        while(1)//Read file
        {
                status = read(inputFd, &lineBuffer[index], sizeof(char));
                if(status == -1)
                {
                        print("Error while reading inputFile: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                        exit(EXIT_FAILURE);
                }

                if(lineBuffer[index] == '\n' && index == 0)//When the first readed char is \n, finish reading florist information
                        break;
                
                if(lineBuffer[index] == '\n')//When new line readed, seek reached at the end of line, process current line
                {
                        lineBuffer[index] = '\0';
                        processFlorist(lineBuffer, &f);
                        if(floristArr == NULL) //If array not initialized initialize
                        {
                                floristArr = (florist*) malloc(sizeof(florist)*(floristNum+1));
                                if(!floristArr)
                                {
                                        print("Error allocating floristArr: line: %d\n", STDERR_FILENO, 1, __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                        }
                        else //Make room for new florist
                        {
                                floristArr = (florist*) realloc(floristArr, sizeof(florist)*(floristNum+1));
                                if(!floristArr)
                                {
                                        print("Error reallocating floristArr: line: %d\n", STDERR_FILENO, 1, __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                        }
                        f.id = floristNum; //Give id to florist
                        floristArr[floristNum] = f;
                        floristNum += 1;
                        index = 0;
                }
                else
                {
                        index++;
                }
        }

        print("%d florist have been created\n", STDOUT_FILENO, 1, floristNum);
        
        customerPerFlorist = (customer**) malloc(sizeof(customer*)*floristNum);
        if(!customerPerFlorist)
        {
                print("Error allocating customerPerFlorist: line: %d\n", STDERR_FILENO, 1, __LINE__);
                exit(EXIT_FAILURE);
        }

        for(int i = 0; i < floristNum; i++)
        {
                customerPerFlorist[i] = (customer*) malloc(sizeof(customer)*5);//Each queue of florist starts with size 5
                if(!customerPerFlorist[i])
                {
                        print("Error allocating customerPerFlorist[i]: line: %d\n", STDERR_FILENO, 1, __LINE__);
                        exit(EXIT_FAILURE); 
                }
        }
        
        requestPerFlorist = (int*) calloc(floristNum,sizeof(int));
        if(!requestPerFlorist)
        {
                print("Error allocating requestPerFlorist: line: %d\n", STDERR_FILENO, 1, __LINE__);
                exit(EXIT_FAILURE); 
        }

        queueSize = (int*) calloc(floristNum,sizeof(int));
        if(!queueSize)
        {
                print("Error allocating queueSize: line: %d\n", STDERR_FILENO, 1, __LINE__);
                exit(EXIT_FAILURE); 
        }

        for(int i = 0; i < floristNum; i++)
        {
                queueSize[i] = 5;
        }

        totalRequest = (customer*) malloc(sizeof(customer)*5);//total request array starting size is 5
        if(!totalRequest)
        {
                print("Error allocating totalRequest: line: %d\n", STDERR_FILENO, 1, __LINE__);
                exit(EXIT_FAILURE); 
        }

        status = pthread_mutex_init(&mutex, NULL); //Initialize mutex
        if(status != 0)
        {
                print("Error initializing mutex: line: %d\n", STDERR_FILENO, 1, __LINE__);
                exit(EXIT_FAILURE);
        }

        status = pthread_mutex_init(&quitMutex, NULL); //Initialize quitMutex
        if(status != 0)
        {
                print("Error initializing mutex: line: %d\n", STDERR_FILENO, 1, __LINE__);
                exit(EXIT_FAILURE);
        }


        condition = calloc(floristNum, sizeof(pthread_cond_t));//create room for condition variables
        if(!condition)
        {
                print("Error allocating condition: line: %d\n", STDERR_FILENO, 1, __LINE__);
                exit(EXIT_FAILURE);
        }
        for(int i = 0; i < floristNum; i++)
        {
                status = pthread_cond_init(&condition[i], NULL); //Initialize condition variables
                if(status != 0)
                {
                        print("Error on pthread_cond_init: line: %d\n", STDERR_FILENO, 1, __LINE__);
                        exit(EXIT_FAILURE);
                }
        }

        floristThreads = (pthread_t*) malloc(sizeof(pthread_t)*floristNum);
        if(!floristThreads)
        {
                print("Error allocating floristThreads: line: %d\n", STDERR_FILENO, 1, __LINE__);
                exit(EXIT_FAILURE);
        }

        for(int i = 0; i < floristNum; i++)
        {
                //Create threads with function floristFunc and send them their structures as argument
                status = pthread_create(&floristThreads[i], NULL, floristFunc, (void*)&floristArr[i]);
                if(status != 0)
                {
                        print("Error while creating florist threads: line: %d\n", STDERR_FILENO, 1, __LINE__);
                        exit(EXIT_FAILURE);
                }
        }

        //Now all data sturctures created, unblock SIGINT
        if(sigprocmask(SIG_UNBLOCK, &set, NULL) == -1)
        {
                print("sigprocmask SIGINT: %s", STDERR_FILENO,1,strerror(errno));                
                _exit(EXIT_FAILURE);
        }
                
        index = 0;
        customer req;

        int current;

        print("Processing requests\n", STDOUT_FILENO, 0);

        while(!quitFlag())//Read customers, until reach end of file or process is signaled by SIGINT
        {
                status = read(inputFd, &lineBuffer[index], sizeof(char));
                if(status == -1)
                {
                        print("Error while reading inputFile: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                        exit(EXIT_FAILURE);
                }

                if(lineBuffer[index] == '\n' && index == 0)//If first readed char is \n finish reading customers
                        break;
                
                if(lineBuffer[index] == '\n')//If readed char is \n, seek reached newline, process current line
                {
                        lineBuffer[index] = '\0';
                        processCustomer(lineBuffer, &req);
                        
                        for(int i = 0; i < floristNum; i++)//Find first florist that has stock for that particular order as candidate
                        {
                                if(inStock(floristArr[i], req.order))
                                {
                                        current = i;
                                        break;
                                }
                        }

                        for(int i = 0; i < floristNum; i++)//compare candidate florist with other to find most closest to customer by looking Chebysev distance
                        {
                                if(inStock(floristArr[i], req.order))
                                {
                                        if(distance(floristArr[i].location, req.location) < distance(floristArr[current].location, req.location))
                                        {
                                                current = i;
                                        }
                                }
                        }

                        if(pthread_mutex_lock(&mutex) != 0)//Own to access shared data
                        {
                                print("Error on pthread_mutex_lock: line: %d\n", STDERR_FILENO, 1, __LINE__);
                                exit(EXIT_FAILURE); 
                        }

                        if(requestPerFlorist[current] == queueSize[current])//If queue is filled, make new room for new request
                        {
                                customerPerFlorist[current] = (customer*)realloc(customerPerFlorist[current], sizeof(customer)*(requestPerFlorist[current]+1));
                                if(!customerPerFlorist[current])
                                {
                                        print("Error reallocating  customerPerFlorist[current]: line: %d\n", STDERR_FILENO, 1, __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                                queueSize[current] += 1;
                        }

                        totalRequest[totalRequestNum] = req;

                        totalRequestNum += 1;

                        //If total request array filled when last request putted, enlarge it with making 5 more room
                        if(totalRequestNum == totalRequestSize)
                        {
                                totalRequest = (customer*) realloc(totalRequest, (totalRequestNum+5)*sizeof(customer));
                                if(!totalRequest)
                                {
                                        print("Error reallocating totalRequest: line: %d\n", STDERR_FILENO, 1, __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                                totalRequestSize += 5;
                        }

                        //Put request to queue and increase # of request for florist
                        customerPerFlorist[current][requestPerFlorist[current]] = req;

                        requestPerFlorist[current] += 1;

                        if(requestPerFlorist[current] == 1)
                        {
                                if(pthread_cond_signal(&condition[current]) != 0)
                                {
                                        print("Error on pthread_cond_signal: line: %d\n", STDERR_FILENO, 1, __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                        }

                        if(pthread_mutex_unlock(&mutex) != 0)//Unlock access
                        {
                                print("Error on pthread_mutex_unlock: line: %d\n", STDERR_FILENO, 1, __LINE__);
                                exit(EXIT_FAILURE); 
                        }

                        index = 0;
                }
                else
                {
                        index++;
                }
        }
        
        if(pthread_mutex_lock(&mutex) != 0)
        {
                print("Error on pthread_mutex_lock: line: %d\n", STDERR_FILENO, 1, __LINE__);
                exit(EXIT_FAILURE); 
        }

        //When all requests are processed or SIGINT signaled, main thread reaches here
        //Puts poison to the queue of each florist just like normal process
        //This poison kill all proccesed thread. When signaled, some threads may check quitFlag and dies but some threads 
        //may be waiting on a condition. So we must put poison in either way to make sure all threads ends.
        char killer[] = "poison"; 
        poison.order = malloc(sizeof(char)*(strlen(killer)+1));
        strcpy(poison.order, killer);
        for(int i = 0; i < floristNum; i++)
        {
                if(requestPerFlorist[i] == queueSize[i])
                {
                        customerPerFlorist[i] = (customer*)realloc(customerPerFlorist[i], sizeof(customer)*(requestPerFlorist[i]+1));
                        if(!customerPerFlorist[i])
                        {
                                print("Error reallocating  customerPerFlorist[i]: line: %d\n", STDERR_FILENO, 1, __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        queueSize[i] += 1;
                }
                customerPerFlorist[i][requestPerFlorist[i]] = poison;
                requestPerFlorist[i] += 1;
                if(requestPerFlorist[i] == 1)
                {
                        pthread_cond_signal(&condition[i]);
                        if(pthread_cond_signal(&condition[i]) != 0)
                        {
                                print("Error on pthread_cond_signal: line: %d\n", STDERR_FILENO, 1, __LINE__);
                                exit(EXIT_FAILURE);
                        }
                }
        }

        if(pthread_mutex_unlock(&mutex) != 0)
        {
                print("Error on pthread_mutex_lock: line: %d\n", STDERR_FILENO, 1, __LINE__);
                exit(EXIT_FAILURE); 
        }
        
        returnF = (florist**)malloc(sizeof(florist*)*floristNum);


        for(int i = 0; i < floristNum; i++)
        {
                //Join all threads to release their resources and take their return values to returnF array in order
                status = pthread_join(floristThreads[i], (void**)&returnF[i]);
                if(status != 0)
                {
                        print("Error on pthread_join: line: %d\n", STDERR_FILENO, 1, __LINE__);
                        exit(EXIT_FAILURE); 
                }
        }

        //If quit flag not setted print results in nice format
        if(!quitFlag())
        {
                print("All requests processed\n", STDOUT_FILENO, 0);
                for(int i = 0; i < floristNum; i++)
                        print("%s closing shop\n", STDOUT_FILENO, 1, returnF[i]->floristName);
                        
                print("Sale statistics for today:\n", STDOUT_FILENO, 0);
                print("------------------------------------------------------------\n", STDOUT_FILENO, 0);
                print("%-20s%-20s%-20s\n", STDOUT_FILENO, 3, "Florist", "# of sales", "Total time");
                print("------------------------------------------------------------\n", STDOUT_FILENO, 0);
                for(int i = 0; i < floristNum; i++)
                {
                        print("%-20s%-20d%-20d\n", STDOUT_FILENO, 3, returnF[i]->floristName, returnF[i]->saleStats.totalSelling, returnF[i]->saleStats.totalTime);
                }
                print("------------------------------------------------------------\n", STDOUT_FILENO, 0);
        }
        exit(EXIT_SUCCESS);
}

void *floristFunc(void *arg)
{
        florist *f = (florist*) arg;
        unsigned int time = 0;
        customer req;
        while(!quitFlag())//Check if it is possible to break loop and join with main thread
        {
                if(pthread_mutex_lock(&mutex) != 0)
                {
                        print("Error on pthread_mutex_unlock: line: %d\n", STDERR_FILENO, 1, __LINE__);
                        exit(EXIT_FAILURE); 
                }

                //If total request for that particular florist is 0, then florist must wait until signaled by main thread.
                while(requestPerFlorist[f->id] == 0)
                {
                        if(pthread_cond_wait(&condition[f->id], &mutex) != 0)
                        {
                                print("Error on pthread_cond_wait: line: %d\n", STDERR_FILENO, 1, __LINE__);
                                exit(EXIT_FAILURE);
                        }
                }
                //If order is poison, florist must break loop and return
                if(strcmp(customerPerFlorist[f->id][0].order, "poison") == 0)
                {
                        if(pthread_mutex_unlock(&mutex) != 0)
                        {
                                print("Error on pthread_mutex_unlock: line: %d\n", STDERR_FILENO, 1, __LINE__);
                                exit(EXIT_FAILURE); 
                        }
                        break;
                }
                
                //preparetion time
                time += (rand() % 250) + 1;

                //Deliver time by simply using t = x/v
                time += distance(f->location, customerPerFlorist[f->id][0].location)/(f->speed);
                
                requestPerFlorist[f->id] -= 1;
                req = customerPerFlorist[f->id][0];
                
                //Since we process queue element, we simply move elements to left by one
                for(int i = 0; i < requestPerFlorist[f->id]; i++)
                        customerPerFlorist[f->id][i] = customerPerFlorist[f->id][i+1];


                f->saleStats.totalSelling += 1;
                f->saleStats.totalTime += time;

                if(pthread_mutex_unlock(&mutex) != 0)
                {
                        print("Error on pthread_mutex_unlock: line: %d\n", STDERR_FILENO, 1, __LINE__);
                        exit(EXIT_FAILURE); 
                }

                usleep(1000*time);//Simulate time by sleeping

                print("Florist %s has delivered a %s to %s in %d ms\n", STDOUT_FILENO, 4, f->floristName, req.order, req.customerName, time);
                time = 0;
        }

        return arg;
}

//Clean all allocated resources and destory mutexes and condition variables
void cleanUp(void)
{
        setFlag();

        if(inputFd != -1)
                close(inputFd);
        
        
        for(int i = 0; i < floristNum; i++)
        {
                free(floristArr[i].floristName);

                pthread_cond_destroy(&condition[i]);
                
                for(int j = 0; j < floristArr[i].numberOfFlower; j++)
                        free(floristArr[i].flowerTypes[j]);

                free(floristArr[i].flowerTypes);

                free(customerPerFlorist[i]);
        }

        pthread_mutex_destroy(&mutex);
        pthread_mutex_destroy(&quitMutex);
        
        free(floristArr);
        
        free(customerPerFlorist);

        for(int i = 0; i < totalRequestNum; i++)
        {
                free(totalRequest[i].customerName);
                free(totalRequest[i].order);
        }

        free(returnF);

        free(requestPerFlorist);

        free(queueSize);
        
        free(floristThreads);

        free(condition);

        free(totalRequest);
        
        free(poison.order);
        
}

//Start termination process
void termHandler(int signo)
{
        setFlag();
        print("Process is shutting down...\n", STDERR_FILENO, 0);
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