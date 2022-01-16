#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>

int print(const char* str, int output, int argNum, ...); //print function

static int liveChild;

pid_t *childs = NULL; //holds pid of child processes
size_t childsIndex = 0;
int inputFd = -1; //deleting opened input file at exit

sem_t *semaphores;
sem_t *mutexes;
size_t mutexNum = 0;
size_t semaphoreNum = 0;

void addChild(pid_t pid); //Add child pid to array to kill when signal delivered

void childHandler(int signo); //SIGCHLD handler for parent

//SIGINT handler for parent. Parent catches and look for childs if they exist still. If they are still exist, sends SIGTERM to them and
//kills them. After that waits for childs and reap all childs.
void termParent(int signo);

//When child gets signal of SIGINT it handles it and send it to Parent, so parent can start exiting process
void sendChild(int signo);

//When childs get signal of SIGTERM they exit gracefully and cleanUp handles all release process 
void termChild(int signo);

void destroyShareMem(); //Destroy shared memories in the list

void destroySemaphoresAndMutexes(); //Destroy all mutex and semaphore at parent to not create undefined behaviour.

int main(int argc, char *argv[])
{
        int opt;
        int err = 0;
        int cookNum, studentNum, tableNum, counterSize, kitchenSize, turnTime;
        int nFlag = 0, mFlag = 0, tFlag = 0, sFlag = 0, lFlag = 0, fFlag = 0;
        char* fileName; //Input file path
        int flags, fd;
        mode_t perms;
        int status = 0;
        pid_t holder; //hold pid of created childs
        
        flags = O_RDWR | O_CREAT;
        perms = S_IRUSR | S_IWUSR;

        setbuf(stdout, NULL); //Disable buffering
        
        while ((opt = getopt(argc, argv, ":N:M:T:S:L:F:")) != -1) //Get arguments and if there is missing print usage
        {
                switch (opt)
                {
                case 'N':
                        nFlag = 1;
                        cookNum = strtol(optarg, NULL, 10);
                        break;
                case 'M':
                        mFlag = 1;
                        studentNum = strtol(optarg, NULL, 10);
                        break;
                case 'T':
                        tFlag = 1;
                        tableNum = strtol(optarg, NULL, 10);
                        break;
                case 'S':
                        sFlag = 1;
                        counterSize = strtol(optarg, NULL, 10);
                        break;
                case 'L':
                        lFlag = 1;
                        turnTime = strtol(optarg, NULL, 10);
                        break;
                case 'F':
                        fFlag = 1;
                        fileName = optarg;
                        break;
                case '?':
                        print("Unknown option: -%c\n", STDERR_FILENO, 1, optopt);
                        err++;
                        break;
                case ':':
                        print("Missing arg for -%c\n", STDERR_FILENO, 1, optopt);
                        err++;
                        break;
                }
        }

        if(nFlag == 0 || mFlag == 0 || tFlag == 0 || sFlag == 0 || lFlag == 0 || fFlag == 0)
        {
                print("Usage: -N [cookNum] -M [studentNum] -T [tableNum] -S [counterSize] -L [turnTime] -F [filePath]\n", STDERR_FILENO, 0);
                _exit(EXIT_FAILURE);
        }

        if(err)
        {
                print("Usage: -N [cookNum] -M [studentNum] -T [tableNum] -S [counterSize] -L [turnTime] -F [filePath]\n", STDERR_FILENO, 0);
                _exit(EXIT_FAILURE);
        }

        /***********Check constraints that given on PDF****************/
        if(cookNum <= 2)
        {
                print("N must be greater than 2", STDERR_FILENO, 0);
                _exit(EXIT_FAILURE);
        }

        if(studentNum <= cookNum)
        {
                print("M must greater than N\n", STDERR_FILENO, 0);
                _exit(EXIT_FAILURE);
        }

        if(counterSize <= 3)
        {
                print("S must greater than 3\n", STDERR_FILENO, 0);
                _exit(EXIT_FAILURE);
        }

        if(tableNum < 1)
        {
                print("T must greater equal than 1\n", STDERR_FILENO, 0);
                _exit(EXIT_FAILURE);
        }

        if(tableNum >= studentNum)
        {
                print("M must greater than T\n", STDERR_FILENO, 0);
                _exit(EXIT_FAILURE);
        }

        if(turnTime < 3)
        {
                print("L must greater equal than 3\n", STDERR_FILENO, 0);
                _exit(EXIT_FAILURE);
        }

        kitchenSize = 2*turnTime*studentNum + 1; //Set kitchen size

        liveChild = cookNum + studentNum;//Number of live childs


        if(atexit(destroyShareMem) < 0)//Register destroyShareMem function. This function clear all resources in any case of exit() calling.
        {
                print("atexit register shared memory destroy error: %s", STDERR_FILENO, 2, strerror(errno), __LINE__);
                _exit(EXIT_FAILURE);
        }

        sigset_t blockMask, emptyMask, intBlock;
        struct sigaction sa;

        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sa.sa_handler = childHandler;           //Set childHandler for SIGCHLD handler
        if(sigaction(SIGCHLD, &sa, NULL) == -1)
        {
                print("sigaction SIGCHLD handler: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        sigemptyset(&blockMask);
        sigaddset(&blockMask, SIGCHLD); //Block SIGCHLD for not to lost and get it at the end of parent
        if(sigprocmask(SIG_SETMASK, &blockMask, NULL) == -1)
        {
                print("block SIGCHLD handler: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        sigemptyset(&intBlock);
        sigaddset(&intBlock, SIGINT);
        sigaddset(&intBlock, SIGTERM);//Also block SIGINT and SIGTERM before forking and not to lost signal.
        if(sigprocmask(SIG_BLOCK, &intBlock, &blockMask) == -1)
        {
                print("block SIGINT handler: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        sa.sa_handler = termParent; //Set termParent for SIGINT handler
        if(sigaction(SIGINT, &sa, NULL) == -1)
        {
                print("sigaction SIGINT handler: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        /******************SHARED OBJECTS**************************/
        sem_t *sem; //shared memory for semaphores
        sem_t *mutex;   //shared memory for mutexes
        int *kitchenStats;      //shared memory that stores kitchen stats
        int *counterStats;      //shared memory that stores counter stats
        int *tableStats;        //shared memory that store tables stats

        /***********************SEMAPHORES************************************/

        fd = shm_open("/shm_semaphores", flags, perms); //Open shared memory

        if(fd == -1)
        {
                print("shm_open 1 error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        if(ftruncate(fd, sizeof(sem)*11) == -1)
        {
                print("truncate 1 error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        sem = (sem_t*) mmap(NULL, sizeof(sem)*11, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); //map shared semaphores to address
        if(sem == MAP_FAILED)
        {
                print("mmap 1 error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        if(close(fd) == -1)
        {
                print("close fd 1 error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        semaphores = sem; //Save address of semaphores to destroy

        /***********************MUTEXES************************************/

        fd = shm_open("/shm_mutex", flags, perms);

        if(fd == -1)
        {
                print("shm_open 2 error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        if(ftruncate(fd, sizeof(mutex)*10) == -1)
        {
                print("truncate 2 error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        mutex = (sem_t*) mmap(NULL, sizeof(mutex)*10, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);//map shared mutexes to address
        if(mutex == MAP_FAILED)
        {
                print("mmap 2 error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        if(close(fd) == -1)
        {
                print("close fd 2 error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        mutexes = mutex; //Save address of mutexes to destroy

        /***********************KITCHEN STATS********************************/


        fd = shm_open("/shm_kitchenStats", flags, perms);

        if(fd == -1)
        {
                print("shm_open 3 error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        if(ftruncate(fd, sizeof(kitchenStats)*4) == -1)
        {
                print("truncate 3 error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        kitchenStats = (int*) mmap(NULL, sizeof(kitchenStats)*4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);//map shared kitchen stats array to address
        if(kitchenStats == MAP_FAILED)
        {
                print("mmap 3 error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        if(close(fd) == -1)
        {
                print("close fd 3 error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        /****************************TABLE STATS*****************************/

        fd = shm_open("/shm_tableStats", flags, perms);

        if(fd == -1)
        {
                print("shm_open 4 error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        if(ftruncate(fd, sizeof(tableStats)*4) == -1)
        {
                print("truncate 4 error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        tableStats = (int*) mmap(NULL, sizeof(tableStats)*4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);//map shared table stats array to address
        if(tableStats == MAP_FAILED)
        {
                print("mmap 4 error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        if(close(fd) == -1)
        {
                print("close fd 4 error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        /***********************COUNTER STATS********************************/


        fd = shm_open("/shm_counterStats", flags, perms);

        if(fd == -1)
        {
                print("shm_open 5 error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        if(ftruncate(fd, sizeof(counterStats)*6) == -1)
        {
                print("truncate 5 error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        counterStats = (int*) mmap(NULL, sizeof(counterStats)*6, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);//map shared counter stats array to address
        if(counterStats == MAP_FAILED)
        {
                print("mmap 5 error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        if(close(fd) == -1)
        {
                print("close fd 5 error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        /*****************************MUTEXES**************************************************/

        if(sem_init(&mutex[0], 1, 1) == -1)//init kitchen stats mutex
        {
                print("kitchen stats access mutex error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        mutexNum++;

        if(sem_init(&mutex[1], 1, 1) == -1)//init counter stats mutex
        {
                print("counter stats access mutex error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        mutexNum++;

        if(sem_init(&mutex[2], 1, 1) == -1)//init access to cooker mutex
        {
                print("cooker access mutex error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        mutexNum++;

        if(sem_init(&mutex[3], 1, 1) == -1)//init access to student mutex
        {
                print("student access mutex error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        mutexNum++;

        if(sem_init(&mutex[4], 1, 1) == -1)//init access to table mutex
        {
                print("table access mutex error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        mutexNum++;

        if(sem_init(&mutex[5], 1, 1) == -1)//init table stats mutex
        {
                print("table stats access mutex error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        mutexNum++;

        if(sem_init(&mutex[6], 1, 0) == -1)//init wait on soup to counter for missing food
        {
                print("soup wait error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        mutexNum++;
        if(sem_init(&mutex[7], 1, 0) == -1)//init wait on soup to counter for missing food
        {
                print("main course wait error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        mutexNum++;
        if(sem_init(&mutex[8], 1, 0) == -1)//init wait on soup to counter for missing food
        {
                print("desert wait error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        mutexNum++;

        /******************************SEMAPHORES******************************************/

        if(sem_init(&sem[0], 1, kitchenSize) == -1)//init kitchen emptyness semaphore
        {
                print("kitchen empty semaphore error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        semaphoreNum++;

        if(sem_init(&sem[1], 1, 0) == -1)//init kitchen fullness semaphore
        {
                print("kitchen full semaphore error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        semaphoreNum++;

        if(sem_init(&sem[2], 1, counterSize) == -1)//init counter emptyness semaphore
        {
                print("counter empty semaphore error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        semaphoreNum++;

        if(sem_init(&sem[3], 1, 0) == -1)//init kitchen fullness semaphore
        {
                print("counter full semaphore error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        semaphoreNum++;

        if(sem_init(&sem[4], 1, 0) == -1)//init number of soup on counter semaphore to wait if it is missing
        {
                print("counter soup semophore error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        semaphoreNum++;

        if(sem_init(&sem[5], 1, 0) == -1)//init number of main course on counter semaphore to wait if it is missing
        {
                print("counter main course semophore error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        semaphoreNum++;

        if(sem_init(&sem[6], 1, 0) == -1)//init number of desert on counter semaphore to wait if it is missing
        {
                print("counter desert semaphore error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        semaphoreNum++;

        if(sem_init(&sem[7], 1, 0) == -1)//init number of soup on counter semaphore to wait if it is missing
        {
                print("kitchen soup semaphore error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        semaphoreNum++;

        if(sem_init(&sem[8], 1, 0) == -1)//init number of main course on counter semaphore to wait if it is missing
        {
                print("kitchen main course semaphore error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        semaphoreNum++;

        if(sem_init(&sem[9], 1, 0) == -1)//init number of desert on counter semaphore to wait if it is missing
        {
                print("kitchen desert semaphore error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        semaphoreNum++;

        if(sem_init(&sem[10], 1, tableNum) == -1)//init number of available table semaphore to wait if all is full
        {
                print("kitchen desert semaphore error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        semaphoreNum++;


        kitchenStats[0] = 0; //Kitchen fulness
        kitchenStats[1] = 0; //soup fulness
        kitchenStats[2] = 0; //mainCourse fulness
        kitchenStats[3] = 0; //desert fulness
        
        counterStats[0] = 0; //Counter fulness
        counterStats[1] = 0; //soup fulness
        counterStats[2] = 0; //mainCourse fulness
        counterStats[3] = 0; //desert fulness
        counterStats[4] = 0; //Student number at counter
        counterStats[5] = 0; //total served

        tableStats[0] = 0; //Fulness of tables
        tableStats[1] = tableNum; //Empty table

        /*********************************COOKERS******************************************/

        for(int i = 0; i < cookNum; i++)
        {
                switch (holder = fork())
                {
                case -1:
                        print("forking %dth cooker process error: %s, line: %d\n", STDERR_FILENO ,2 , i, strerror(errno), __LINE__);
                        exit(EXIT_FAILURE);
                        break;
                case 0:
                        /*************************SIGNAL HANDLERS SETTINGS**********************/
                        sa.sa_handler = sendChild; //Set sendChild  as SIGINT handler 
                        if(sigaction(SIGINT, &sa, NULL) == -1)
                        {
                                print("sigaction SIGINT handler: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        sa.sa_handler = termChild; //Set termChild  as SIGTERM handler
                        if(sigaction(SIGTERM, &sa, NULL) == -1)
                        {
                                print("sigaction SIGTERM handler: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        if(sigprocmask(SIG_UNBLOCK, &intBlock, NULL) == -1) //Signal handlers are setted. Now we can unblock them.
                        {
                                print("unblock SIGINT p2 handler:%s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        /**********************************************************************/

                        while (1)
                        {
                                int takeFood; //Food type that taken or should be taken
                                while((status = sem_wait(&mutex[2])) == -1 && errno == EINTR)//Access to cook
                                        continue;
                                if(status == -1)
                                {
                                        print("sem_wait mutex2 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                                
                                while((status = sem_wait(&mutex[1])) == -1 && errno == EINTR)//access to counter stats
                                        continue;
                                if(status == -1)
                                {
                                        print("sem_wait mutex1 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                                
                                if(counterStats[5] == 3*turnTime*studentNum) //If all foods are served, break aand start termination
                                {
                                        if(sem_post(&mutex[1]) == -1)
                                        {
                                                print("sem_post mutex1 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }
                                        
                                        if(sem_post(&mutex[2]) == -1)
                                        {
                                                print("sem_post mutex2 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }
                                        break;
                                }
                                if(counterStats[0]+1 == counterSize || counterStats[0]+2 == counterSize)//If there is few spaces on counter and still some foods are missing, determine them and wait for them
                                {
                                        if(counterStats[1] == 0)
                                        {
                                                if(sem_post(&mutex[1]) == -1)
                                                {
                                                        print("sem_post mutex1 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                        exit(EXIT_FAILURE);
                                                }
                                                
                                                while((status = sem_wait(&sem[7])) == -1 && errno == EINTR)
                                                        continue;
                                                if(status == -1)
                                                {
                                                        print("sem_wait sem7 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                        exit(EXIT_FAILURE);
                                                }
                                                
                                                while((status = sem_wait(&sem[1])) == -1 && errno == EINTR)
                                                        continue;
                                                if(status == -1)
                                                {
                                                        print("sem_wait sem1 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                        exit(EXIT_FAILURE);
                                                }

                                                takeFood = 0; //soup missing take it

                                                if(sem_post(&sem[7]) == -1)
                                                {
                                                        print("sem_post sem7 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                        exit(EXIT_FAILURE);
                                                }
                                        }
                                        else if(counterStats[2] == 0)
                                        {
                                                if(sem_post(&mutex[1]) == -1)
                                                {
                                                        print("sem_post mutex1 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                        exit(EXIT_FAILURE);
                                                }
                                                
                                                while((status = sem_wait(&sem[8])) == -1 && errno == EINTR)
                                                        continue;
                                                if(status == -1)
                                                {
                                                        print("sem_wait sem8 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                        exit(EXIT_FAILURE);
                                                }

                                                while((status = sem_wait(&sem[1])) == -1 && errno == EINTR)
                                                        continue;
                                                if(status == -1)
                                                {
                                                        print("sem_wait sem1 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                        exit(EXIT_FAILURE);
                                                }

                                                takeFood = 1; //main course missing take it

                                                if(sem_post(&sem[8]) == -1)
                                                {
                                                        print("sem_post sem8 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                        exit(EXIT_FAILURE);
                                                }
                                        }
                                        else if(counterStats[3] == 0)
                                        {
                                                if(sem_post(&mutex[1]) == -1)
                                                {
                                                        print("sem_post mutex1 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                        exit(EXIT_FAILURE);
                                                }
                                                        
                                                while((status = sem_wait(&sem[9])) == -1 && errno == EINTR)
                                                        continue;
                                                if(status == -1)
                                                {
                                                        print("sem_wait sem9 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                        exit(EXIT_FAILURE);
                                                }

                                                while((status = sem_wait(&sem[1])) == -1 && errno == EINTR)
                                                        continue;
                                                if(status == -1)
                                                {
                                                        print("sem_wait sem1 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                        exit(EXIT_FAILURE);
                                                }

                                                takeFood = 2; //desert is missing take it

                                                if(sem_post(&sem[9]) == -1)
                                                {
                                                        print("sem_post sem9 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                        exit(EXIT_FAILURE);
                                                }
                                        }
                                        else//everything normal take it normally
                                        {
                                                if(sem_post(&mutex[1]) == -1)
                                                {
                                                        print("sem_post mutex1 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                        exit(EXIT_FAILURE);
                                                }
                                                
                                                while((status = sem_wait(&sem[1])) == -1 && errno == EINTR)
                                                        continue;
                                                if(status == -1)
                                                {
                                                        print("sem_wait sem1 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                        exit(EXIT_FAILURE);
                                                }

                                                while((status = sem_wait(&mutex[0])) == -1 && errno == EINTR)
                                                        continue;
                                                if(status == -1)
                                                {
                                                        print("sem_wait mutex0 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                        exit(EXIT_FAILURE);
                                                }

                                                if(kitchenStats[1] != 0)
                                                        takeFood = 0; //take soup if it is on kitchen
                                                else if(kitchenStats[2] != 0)
                                                        takeFood = 1; //take main course if it is on kitchen
                                                else if(kitchenStats[3] != 0)
                                                        takeFood = 2; //take desert if it is on kitchen
                                                
                                                if(sem_post(&mutex[0]) == -1)
                                                {
                                                        print("sem_post mutex0 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                        exit(EXIT_FAILURE);
                                                }
                                        }    
                                }
                                else //everthing normal take some food type that is available
                                {
                                        if(sem_post(&mutex[1]) == -1)
                                        {
                                                print("sem_post mutex1 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }
                                        
                                        while((status = sem_wait(&sem[1])) == -1 && errno == EINTR)
                                                continue;
                                        if(status == -1)
                                        {
                                                print("sem_wait sem1 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }

                                        while((status = sem_wait(&mutex[0])) == -1 && errno == EINTR)
                                                continue;
                                        if(status == -1)
                                        {
                                                print("sem_wait mutex0 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }

                                        if(kitchenStats[1] != 0)
                                                takeFood = 0; //take soup if it is on kitchen
                                        else if(kitchenStats[2] != 0)
                                                takeFood = 1; //take main course if it is on kitchen
                                        else if(kitchenStats[3] != 0)
                                                takeFood = 2; //take desert if it is on kitchen
                                        
                                        if(sem_post(&mutex[0]) == -1)
                                        {
                                                print("sem_post mutex0 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }
                                }

                                if(takeFood == 0)// take soup on kitchen
                                {
                                        while((status = sem_wait(&mutex[0])) == -1 && errno == EINTR)//access kitchen stats
                                                continue;
                                        if(status == -1)
                                        {
                                                print("sem_wait mutex0 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }

                                        while((status = sem_wait(&sem[7])) == -1 && errno == EINTR)//take soup from kitchen
                                                continue;
                                        if(status == -1)
                                        {
                                                print("sem_wait sem7 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }

                                        print("Cook %d is going to the kitchen to wait for/get a plate: kitchen items P:%d, C:%d, D:%d = %d\n", STDOUT_FILENO, 5, i,kitchenStats[1], kitchenStats[2], kitchenStats[3], kitchenStats[0]);
                                        kitchenStats[0] -=1;
                                        kitchenStats[1] -=1;
                                        if(sem_post(&mutex[0]) == -1)
                                        {
                                                print("sem_post mutex0 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }
                                        if(sem_post(&sem[0]) == -1)
                                        {
                                                print("sem_post sem0 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }

                                        while((status = sem_wait(&sem[2])) == -1 && errno == EINTR)//wait empty space at counter
                                                continue;
                                        if(status == -1)
                                        {
                                                print("sem_wait sem2 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }

                                        while((status = sem_wait(&mutex[1])) == -1 && errno == EINTR)
                                                continue;
                                        if(status == -1)
                                        {
                                                print("sem_wait mutex1 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }

                                        print("Cook %d is going to the counter to deliver soup - counter items P:%d, C:%d, D:%d=%d\n", STDOUT_FILENO, 5, i, counterStats[1], counterStats[2], counterStats[3],counterStats[0]);
                                        counterStats[1] += 1;
                                        counterStats[0] += 1;
                                        counterStats[5] += 1;
                                        
                                        if(sem_post(&sem[4]) == -1)//increase counter semaphore for soup
                                        {
                                                print("sem_post sem4 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }
                                        
                                        print("Cook %d placed soup on the counter - counter items P:%d, C:%d, D:%d=%d\n", STDOUT_FILENO, 5, i, counterStats[1], counterStats[2], counterStats[3],counterStats[0]);
                                        
                                        if(sem_post(&mutex[1]) == -1)
                                        {
                                                print("sem_post mutex1 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }
                                        if(sem_post(&sem[3]) == -1)//we filled counter with one item
                                        {
                                                print("sem_post sem3 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }
                                }
                                else if(takeFood == 1)// take main course on kitchen
                                {
                                        while((status = sem_wait(&mutex[0])) == -1 && errno == EINTR) //access kitchen stats
                                                continue;
                                        if(status == -1)
                                        {
                                                print("sem_wait mutex0 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }

                                        while((status = sem_wait(&sem[8])) == -1 && errno == EINTR) //take main course from kitchen
                                                continue;
                                        if(status == -1)
                                        {
                                                print("sem_wait sem8 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }

                                        print("Cook %d is going to the kitchen to wait for/get a plate: kitchen items P:%d, C:%d, D:%d = %d\n", STDOUT_FILENO, 5, i,kitchenStats[1], kitchenStats[2], kitchenStats[3], kitchenStats[0]);
                                        kitchenStats[0] -=1;
                                        kitchenStats[2] -=1;

                                        if(sem_post(&mutex[0]) == -1)
                                        {
                                                print("sem_post mutex0 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }
                                        if(sem_post(&sem[0]) == -1)
                                        {
                                                print("sem_post sem0 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }

                                        while((status = sem_wait(&sem[2])) == -1 && errno == EINTR)//wait empty space at counter
                                                continue;
                                        if(status == -1)
                                        {
                                                print("sem_wait sem2 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }

                                        while((status = sem_wait(&mutex[1])) == -1 && errno == EINTR)
                                                continue;
                                        if(status == -1)
                                        {
                                                print("sem_wait mutex1 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }

                                        print("Cook %d is going to the counter to deliver main course - counter items P:%d, C:%d, D:%d=%d\n", STDOUT_FILENO, 5, i, counterStats[1], counterStats[2], counterStats[3],counterStats[0]);
                                        counterStats[2] += 1;
                                        counterStats[0] += 1;
                                        counterStats[5] += 1;

                                        if(sem_post(&sem[5]) == -1)//increase counter semaphore for main course
                                        {
                                                print("sem_post sem5 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }

                                        print("Cook %d placed main course on the counter - counter items P:%d, C:%d, D:%d=%d\n", STDOUT_FILENO, 5, i, counterStats[1], counterStats[2], counterStats[3],counterStats[0]);
                                        
                                        if(sem_post(&mutex[1]) == -1)
                                        {
                                                print("sem_post mutex1 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }
                                        if(sem_post(&sem[3]) == -1)//we filled counter with one item
                                        {
                                                print("sem_post sem3 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }
                                }
                                else if(takeFood == 2) //take desert on counter
                                {
                                        while((status = sem_wait(&mutex[0])) == -1 && errno == EINTR)//access kitchen stats
                                                continue;
                                        if(status == -1)
                                        {
                                                print("sem_wait mutex0 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }

                                        while((status = sem_wait(&sem[9])) == -1 && errno == EINTR) //take desert from kitchen
                                                continue;
                                        if(status == -1)
                                        {
                                                print("sem_wait sem9 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }

                                        print("Cook %d is going to the kitchen to wait for/get a plate: kitchen items P:%d, C:%d, D:%d = %d\n", STDOUT_FILENO, 5, i,kitchenStats[1], kitchenStats[2], kitchenStats[3], kitchenStats[0]);
                                        kitchenStats[0] -=1;
                                        kitchenStats[3] -=1;
                                        
                                        if(sem_post(&mutex[0]) == -1)
                                        {
                                                print("sem_post mutex0 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }
                                        if(sem_post(&sem[0]) == -1)
                                        {
                                                print("sem_post sem0 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }

                                        while((status = sem_wait(&sem[2])) == -1 && errno == EINTR)//wait empty space at counter
                                                continue;
                                        if(status == -1)
                                        {
                                                print("sem_wait sem2 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }

                                        while((status = sem_wait(&mutex[1])) == -1 && errno == EINTR)
                                                continue;
                                        if(status == -1)
                                        {
                                                print("sem_wait mutex1 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }
                                        
                                        print("Cook %d is going to the counter to deliver desert - counter items P:%d, C:%d, D:%d=%d\n", STDOUT_FILENO, 5, i, counterStats[1], counterStats[2], counterStats[3],counterStats[0]);
                                        counterStats[3] += 1;
                                        counterStats[0] += 1;
                                        counterStats[5] += 1;

                                        if(sem_post(&sem[6]) == -1)//increase counter semaphore for desert
                                        {
                                                print("sem_post sem6 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }
                                        
                                        print("Cook %d placed desert on the counter - counter items P:%d, C:%d, D:%d=%d\n", STDOUT_FILENO, 5, i, counterStats[1], counterStats[2], counterStats[3],counterStats[0]);
                                        
                                        if(sem_post(&mutex[1]) == -1)
                                        {
                                                print("sem_post mutex1 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }
                                        if(sem_post(&sem[3]) == -1)//we filled counter with one item
                                        {
                                                print("sem_post sem3 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                                exit(EXIT_FAILURE);
                                        }
                                }
                                if(sem_post(&mutex[2]) == -1)
                                {
                                        print("sem_post mutex2 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                        }
                        while((status = sem_wait(&mutex[0])) == -1 && errno == EINTR)
                                continue;
                        if(status == -1)
                        {
                                print("sem_wait mutex0 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        
                        print("Cook %d finished serving - items at kitchen: %d -going home - GOODBYE!!!\n", STDOUT_FILENO, 2, i, kitchenStats[0]);
                        
                        if(sem_post(&mutex[0]) == -1)
                        {
                                print("sem_post mutex0 inside cooker error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }

                        exit(EXIT_SUCCESS);
                default:
                        addChild(holder);
                        break;
                }
        }

        /*********************************STUDENTS******************************************/
        int studentTable = -1; //table that student sit
        for(int i = 0; i < studentNum; i++)
        {
                switch (holder = fork())
                {
                case -1:
                        print("forking %dth student process error: %s, line: %d\n", STDERR_FILENO ,2 , i, strerror(errno), __LINE__);
                        exit(EXIT_FAILURE);
                        break;
                case 0:
                        /*************************SIGNAL HANDLERS SETTINGS**********************/
                        sa.sa_handler = sendChild; //Set sendChild  as SIGINT handler 
                        if(sigaction(SIGINT, &sa, NULL) == -1)
                        {
                                print("sigaction SIGINT handler: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        sa.sa_handler = termChild; //Set termChild  as SIGTERM handler
                        if(sigaction(SIGTERM, &sa, NULL) == -1)
                        {
                                print("sigaction SIGTERM handler: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        if(sigprocmask(SIG_UNBLOCK, &intBlock, NULL) == -1) //Signal handlers are setted. Now we can unblock them.
                        {
                                print("unblock SIGINT p2 handler:%s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        /**********************************************************************/

                        for(int j = 0; j < turnTime; j++)
                        {
                                
                                while((status = sem_wait(&mutex[1])) == -1 && errno == EINTR)//access to counter
                                        continue;
                                if(status == -1)
                                {
                                        print("sem_wait mutex1 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }

                                counterStats[4] += 1;
                                print("Student %d is going to counter (round %d) - # of students at counter: %d and counter items P:%d, C:%d, D:%d=%d\n", STDOUT_FILENO, 7, i, j+1, counterStats[4],counterStats[1], counterStats[2], counterStats[3], counterStats[0]);
                                
                                if(sem_post(&mutex[1]) == -1)
                                {
                                        print("sem_post mutex1 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }

                                while((status = sem_wait(&mutex[3])) == -1 && errno == EINTR)// one student goes into counter other must wait until student take 3 plate
                                        continue;
                                if(status == -1)
                                {
                                        print("sem_wait mutex3 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }

                                while((status = sem_wait(&sem[4])) == -1 && errno == EINTR)//take soup from counter if it is there otherwise wait
                                        continue;
                                if(status == -1)
                                {
                                        print("sem_wait sem4 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                                while((status = sem_wait(&sem[5])) == -1 && errno == EINTR)//take main course from counter if it is there otherwise wait
                                        continue;
                                if(status == -1)
                                {
                                        print("sem_wait sem5 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                                
                                while((status = sem_wait(&sem[6])) == -1 && errno == EINTR)//take desert from counter if it is there otherwise wait
                                        continue;
                                if(status == -1)
                                {
                                        print("sem_wait sem6 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                                
                                while((status = sem_wait(&sem[3])) == -1 && errno == EINTR)//we take plates now we can empty counter
                                        continue;
                                if(status == -1)
                                {
                                        print("sem_wait sem3 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                                
                                while((status = sem_wait(&sem[3])) == -1 && errno == EINTR)//we take plates now we can empty counter
                                        continue;
                                if(status == -1)
                                {
                                        print("sem_wait sem3 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                               
                                while((status = sem_wait(&sem[3])) == -1 && errno == EINTR)//we take plates now we can empty counter
                                        continue;
                                if(status == -1)
                                {
                                        print("sem_wait sem3 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                                
                                while((status = sem_wait(&mutex[1])) == -1 && errno == EINTR)
                                        continue;
                                if(status == -1)
                                {
                                        print("sem_wait mutex1 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }

                                counterStats[0] -= 3;
                                counterStats[1] -= 1;
                                counterStats[2] -= 1;
                                counterStats[3] -= 1;
                                counterStats[4] -= 1;
                                
                                if(sem_post(&mutex[1]) == -1)
                                {
                                        print("sem_post mutex1 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }

                                if(sem_post(&sem[2]) == -1)//we take plates now we can empty counter
                                {
                                        print("sem_post sem2 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                                if(sem_post(&sem[2]) == -1)//we take plates now we can empty counter
                                {
                                        print("sem_post sem2 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                                if(sem_post(&sem[2]) == -1)//we take plates now we can empty counter
                                {
                                        print("sem_post sem2 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                                
                                if(sem_post(&mutex[3]) == -1)
                                {
                                        print("sem_post mutex3 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                                while((status = sem_wait(&mutex[5])) == -1 && errno == EINTR)
                                        continue;
                                if(status == -1)
                                {
                                        print("sem_wait mutex5 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                                
                                print("Student %d got food and is going to get a table (round %d) - # of empty tables:%d\n",STDOUT_FILENO, 3, i,j+1,tableStats[1]);
                                
                                if(sem_post(&mutex[5]) == -1)
                                {
                                        print("sem_post mutex5 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }

                                while((status = sem_wait(&sem[10])) == -1 && errno == EINTR)//Students must wait for empty tables with semaphore value
                                        continue;
                                if(status == -1)
                                {
                                        print("sem_wait sem10 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }

                                while((status = sem_wait(&mutex[5])) == -1 && errno == EINTR)
                                        continue;
                                if(status == -1)
                                {
                                        print("sem_wait mutex5 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }

                                studentTable = tableStats[0];
                                tableStats[1] -= 1;
                                print("Student %d sat at table %d to eat (round %d) - # of empty tables:%d\n",STDOUT_FILENO, 4, i,tableStats[0],j+1,tableStats[1]);
                                tableStats[0] += 1;
                                
                                if(sem_post(&mutex[5]) == -1)
                                {
                                        print("sem_post mutex5 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }

                                /*******************************EATING PROCESS may be simulated by putting there a sleep**********************/
                                //sleep(1);
                                
                                while((status = sem_wait(&mutex[5])) == -1 && errno == EINTR)
                                        continue;
                                if(status == -1)
                                {
                                        print("sem_wait mutex5 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }

                                tableStats[1] += 1;
                                print("Student %d left table %d to eat again (round %d) - # of empty tables:%d\n",STDOUT_FILENO, 4,i,studentTable,j+1,tableStats[1]);
                                tableStats[0] -= 1;
                                
                                if(sem_post(&sem[10]) == -1)//one table more is empty now
                                {
                                        print("sem_post sem10 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                                
                                if(sem_post(&mutex[5]) == -1)
                                {
                                        print("sem_post mutex5 inside student error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);
                                }
                        }
                        print("Student %d is done eating L=%d times - going home - GOODBYE!!! \n",STDOUT_FILENO, 2, i, turnTime);
                        exit(EXIT_SUCCESS);
                default:
                        addChild(holder);
                        break;
                }
        }

        if(atexit(destroySemaphoresAndMutexes) < 0)//Register destroySemaphoresAndMutexes function. This function destroys all mutexes and semaphores in any case of exit() calling.
        {
                print("atexit register semaphore destroy error: %s", STDERR_FILENO, 2, strerror(errno), __LINE__);
                _exit(EXIT_FAILURE);
        }

        if(sigprocmask(SIG_UNBLOCK, &intBlock, NULL) == -1) //Signal handlers are setted. Now we can unblock them.
        {
                print("unblock SIGINT p2 handler:%s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        char buf[1];
        if((fd = open(fileName, O_RDONLY)) == -1) //open input file
        {
                print("opening input file error:%s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        /******************CHECK FILE IS CONTAIN EXACTLY L*M ITEM FOR EACH TYPE OF FOOD**************/
        int P = 0, C = 0, D = 0;
        while((status = read(fd, buf, 1)) != 0 && status != -1)
        {
                if(buf[0] == 'P')
                        P++;
                else if(buf[0] == 'C')
                        C++;
                else if(buf[0] == 'D')
                        D++;
        }
        if(status == -1)
        {
                print("reading input file error:%s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        if(P != studentNum*turnTime)//not enough soup
        {
                print("need exactly L*M soups in input file error:%s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        if(C != studentNum*turnTime)//not enough main course
        {
                print("need exactly L*M main courses in input file error:%s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        if(D != studentNum*turnTime)//not enough desert
        {
                print("need exactly L*M deserts in input file error:%s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        if(lseek(fd, 0, SEEK_SET) == -1)//Seek beginning we will read again whole file
        {
                print("seek input file error:%s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        inputFd = fd; //for deleting later
        /**********************************************************************************************/

        while((status = read(fd, buf, 1)) != 0 && status != -1)
        {
                if(buf[0] == 'P')
                {
                        while((status = sem_wait(&mutex[0])) == -1 && errno == EINTR)//access to kitchen
                                continue;
                        if(status == -1)
                        {
                                print("sem_wait mutex0 inside supplier error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        
                        print("The supplier is going to the kitchen to deliver soup: kitchen items P:%d, C:%d, D:%d = %d\n", STDOUT_FILENO, 4, kitchenStats[1], kitchenStats[2], kitchenStats[3], kitchenStats[0]);
                        
                        if(sem_post(&mutex[0]) == -1)
                        {
                                print("sem_post mutex0 inside suuplier error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }

                        while((status = sem_wait(&sem[0])) == -1 && errno == EINTR)//wait for empty space in kitchen
                                continue;
                        if(status == -1)
                        {
                                print("sem_wait sem0 inside supplier error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }

                        while((status = sem_wait(&mutex[0])) == -1 && errno == EINTR)
                                continue;
                        if(status == -1)
                        {
                                print("sem_wait mutex0 inside supplier error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        
                        kitchenStats[0] += 1;
                        kitchenStats[1] += 1;
                        
                        if(sem_post(&sem[7]) == -1)//kitchen has at least one soup now, inform if cooker waits for that
                        {
                                print("sem_post sem7 inside suuplier error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        
                        print("The supplier delivered soup - after delivery: kitchen items P:%d, C:%d, D:%d = %d\n", STDOUT_FILENO, 4, kitchenStats[1], kitchenStats[2], kitchenStats[3], kitchenStats[0]);
                        
                        if(sem_post(&mutex[0]) == -1)
                        {
                                print("sem_post mutex0 inside suuplier error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        
                        if(sem_post(&sem[1]) == -1)//now kitchen has one more item, cooker can take.
                        {
                                print("sem_post sem1 inside suuplier error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                }
                else if(buf[0] == 'C')
                {
                        while((status = sem_wait(&mutex[0])) == -1 && errno == EINTR)//access to kitchen
                                continue;
                        if(status == -1)
                        {
                                print("sem_wait mutex0 inside supplier error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }

                        print("The supplier is going to the kitchen to deliver main course: kitchen items P:%d, C:%d, D:%d = %d\n", STDOUT_FILENO, 4, kitchenStats[1], kitchenStats[2], kitchenStats[3], kitchenStats[0]);
                        
                        if(sem_post(&mutex[0]) == -1)
                        {
                                print("sem_post mutex0 inside suuplier error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }

                        while((status = sem_wait(&sem[0])) == -1 && errno == EINTR)
                                continue;
                        if(status == -1)
                        {
                                print("sem_wait sem0 inside supplier error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }

                        while((status = sem_wait(&mutex[0])) == -1 && errno == EINTR)
                                continue;
                        if(status == -1)
                        {
                                print("sem_wait mutex0 inside supplier error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }

                        kitchenStats[0] += 1;
                        kitchenStats[2] += 1;
                        
                        if(sem_post(&sem[8]) == -1)//kitchen has at least one main course now, inform if cooker waits for that
                        {
                                print("sem_post sem8 inside suuplier error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        
                        print("The supplier delivered main course - after delivery: kitchen items P:%d, C:%d, D:%d = %d\n", STDOUT_FILENO, 4, kitchenStats[1], kitchenStats[2], kitchenStats[3], kitchenStats[0]);
                        
                        if(sem_post(&mutex[0]) == -1)
                        {
                                print("sem_post mutex0 inside suuplier error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        
                        if(sem_post(&sem[1]) == -1)//now kitchen has one more item, cooker can take.
                        {
                                print("sem_post sem1 inside suuplier error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                }
                else if (buf[0] == 'D')
                {
                        while((status = sem_wait(&mutex[0])) == -1 && errno == EINTR)//access to kitchen
                                continue;
                        if(status == -1)
                        {
                                print("sem_wait mutex0 inside supplier error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }

                        print("The supplier is going to the kitchen to deliver desert: kitchen items P:%d, C:%d, D:%d = %d\n", STDOUT_FILENO, 4, kitchenStats[1], kitchenStats[2], kitchenStats[3], kitchenStats[0]);
                        
                        if(sem_post(&mutex[0]) == -1)
                        {
                                print("sem_post mutex0 inside suuplier error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }

                        while((status = sem_wait(&sem[0])) == -1 && errno == EINTR)
                                continue;
                        if(status == -1)
                        {
                                print("sem_wait sem0 inside supplier error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }

                        while((status = sem_wait(&mutex[0])) == -1 && errno == EINTR)
                                continue;
                        if(status == -1)
                        {
                                print("sem_wait mutex0 inside supplier error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }

                        kitchenStats[0] += 1;
                        kitchenStats[3] += 1;
                        
                        if(sem_post(&sem[9]) == -1)//kitchen has at least one desert now, inform if cooker waits for that
                        {
                                print("sem_post sem9 inside suuplier error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        
                        print("The supplier delivered desert - after delivery: kitchen items P:%d, C:%d, D:%d = %d\n", STDOUT_FILENO, 4, kitchenStats[1], kitchenStats[2], kitchenStats[3], kitchenStats[0]);
                        
                        if(sem_post(&mutex[0]) == -1)
                        {
                                print("sem_post mutex0 inside suuplier error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                        
                        if(sem_post(&sem[1]) == -1)//now kitchen has one more item, cooker can take.
                        {
                                print("sem_post sem1 inside suuplier error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE);
                        }
                }
        }

        if(status == -1)
        {
                print("reading input file error:%s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }
        
        print("The supplier finished supplying - GOODBYE!\n", STDOUT_FILENO, 0);

        sigemptyset(&emptyMask);
        while(liveChild > 0)
        {
                //Suspend parent process until receive SIGCHILD delivered.
                //From there by SIGCHLD handler reap childs and take their status values.
                if(sigsuspend(&emptyMask) == -1 && errno != EINTR) 
                {
                        print("sisgsuspen emptyMask error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                        exit(EXIT_FAILURE);
                }
        }
        exit(EXIT_SUCCESS);

}

int print(const char* str, int output, int argNum, ...)
{
        char* line;
        va_list argList;
        va_start(argList, argNum);
        int size = vsnprintf(NULL, 0, str, argList);
        va_end(argList);

        line = (char*)malloc(sizeof(char)*(size+1));
        if(!line)
        {
                write(STDERR_FILENO, "print line allocation error\n", 28);
                exit(EXIT_FAILURE);
        }
        va_start(argList, argNum);
        if(vsnprintf(line, size+1, str, argList) <= 0)
        {
                free(line);
                write(STDERR_FILENO, "format output error\n", 20);
                exit(EXIT_FAILURE);
        }

        if(write(output, line, size) == -1)
        {
                free(line);
                write(STDERR_FILENO, "print write error\n", 18);
                exit(EXIT_FAILURE);
        }
        free(line);
        return size;
}

void addChild(pid_t pid)//add child to pid list
{
        if(childs == NULL)
        {
                childs = (pid_t*)malloc(sizeof(pid_t)*liveChild);
                if(!childs)
                {
                        print("Error on allocating child pid's array: %s, line:%d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                        exit(EXIT_FAILURE);
                }
        }
        childs[childsIndex++] = pid;
}

void childHandler(int signo)
{
        int status, savedErr;
        pid_t child;

        savedErr = errno;

        while((child = waitpid(-1, &status, WNOHANG)) > 0)
                liveChild--;
        
        if(child == -1 && errno != ECHILD)
                print("waitpid handler error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
        
        errno = savedErr;

}

//SIGINT handler for parent. Parent catches and look for childs if they exist still. If they are still exist, sends SIGTERM to them and
//kills them. After that waits for childs and reap all childs.
void termParent(int signo) 
{
        int status;
        pid_t child;

        print("Parent Catched\n", STDERR_FILENO, 0);
        for(size_t i = 0; i < childsIndex; i++)
        {
                if(kill(childs[i], SIGTERM) < 0 && errno != ESRCH) //If process exist send SIGTERM and start termination of child
                {
                        print("sigterm handler error: %s", STDERR_FILENO, 1, strerror(errno));
                        exit(EXIT_FAILURE);
                }
        }
        while((child = waitpid(-1, &status, WNOHANG)) > 0)
                liveChild--;
        if(child == -1 && errno != ECHILD)
        {
                print("waitpid handler error: %s", STDERR_FILENO, 1, strerror(errno));
                exit(EXIT_FAILURE);
        }
        exit(errno);
}

//When child gets signal of SIGINT it handles it and send it to Parent, so parent can start exiting process
void sendChild(int signo)
{
        print("Child Catched\n", STDERR_FILENO, 0);
        kill(getppid(), SIGINT);
}

//When childs get signal of SIGTERM they exit gracefully and cleanUp handles all release process 
void termChild(int signo)
{
        exit(EXIT_FAILURE);
}

void destroyShareMem() //Destroy shared memories in the list
{
        if(inputFd != -1)
                close(inputFd);
        free(childs);
        shm_unlink("/shm_semaphores");
        shm_unlink("/shm_mutex");
        shm_unlink("/shm_kitchenStats");
        shm_unlink("/shm_counterStats");
        shm_unlink("/shm_tableStats");
}

//Destroy all mutex and semaphore at parent to not create undefined behaviour.
void destroySemaphoresAndMutexes()
{
        for(size_t i = 0; i < mutexNum; i++)
                if(sem_destroy(&mutexes[i]) == -1)
                {
                        print("sem_destroy error atexit: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                        exit(EXIT_FAILURE);
                }

        for(size_t i = 0; i < semaphoreNum; i++)
                if(sem_destroy(&semaphores[i]) == -1)
                {
                        print("sem_destroy error atexit: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                        exit(EXIT_FAILURE);
                }
}
