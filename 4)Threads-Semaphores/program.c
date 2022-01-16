#include<pthread.h>
#include <semaphore.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include "semun.h"

char ingredients[2];
int semid;
sem_t gullacLock;

int fd;

int print(const char* str, int output, int argNum, ...); //print function
void *chefThread(void *arg); //Chef thread function
void cleanUp(); //Cleanup function on exit situation.

int main(int argc, char *argv[])
{
        pthread_t chef1, chef2, chef3, chef4, chef5, chef6; //thread structures
        int c1 = 1, c2 = 2, c3 = 3, c4 = 4, c5 = 5, c6 = 6; //chef id
        int status, iFlag = 0, err = 0, c, lineNum = 0; //errors for command line and input file
        void *returnStatus;
        int *returnStatusInt;
        char *input; //input file name
        char buffer[3]; //input buffer
        struct semid_ds ds; //Data structure of systemV semaphore
        union semun arg; //Semun union
        struct sembuf opIngredient[4]; //operation array

        srand(time(NULL));

        /*********************GET ARGUMENTS******************************/
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
                print("USAGE: ./program -i [fileName]\n", STDERR_FILENO, 0);
                _exit(EXIT_FAILURE);
        }
        else if (err)
        {
                print("USAGE: ./program -i [fileName]\n", STDERR_FILENO, 0);
                _exit(EXIT_FAILURE);
        }

        /****************************************************************/

        fd = open(input, O_RDONLY);
        if(fd == -1)
        {
                print("open error: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                _exit(EXIT_FAILURE); 
        }

        /*****************CHECK FILE is VALID or not*********************************/
        while((status = read(fd, buffer, 3)) != 0 && status != -1)
        {
                if(status == 3)
                {
                        if(buffer[2] != '\n')//If third readed char is not newline there is error on input file
                        {
                                if(close(fd) == -1)
                                {
                                        print("closing input error: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno),__LINE__);
                                        _exit(EXIT_FAILURE);
                                }
                                print("file not in right format, line: %d\n", STDERR_FILENO, 1, lineNum+1);
                                _exit(EXIT_FAILURE);  
                        }
                }
                if(buffer[0] == buffer[1])//Ingredients that served not distinct
                {
                        if(close(fd) == -1)
                        {
                                print("closing input error: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno),__LINE__);
                                _exit(EXIT_FAILURE);
                        }
                        print("Two same ingredient in one deliver in file, they must be distinct, line: %d\n", STDERR_FILENO, 1, lineNum+1);
                        _exit(EXIT_FAILURE);
                }
                if(buffer[0] != 'M' && buffer[0] != 'S' && buffer[0] != 'F' && buffer[0] != 'W')//Unknown ingredient
                {
                        if(close(fd) == -1)
                        {
                                print("closing input error: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno),__LINE__);
                                _exit(EXIT_FAILURE);
                        }
                        print("Unknown ingredient, line: %d\n", STDERR_FILENO, 1,lineNum+1);
                        _exit(EXIT_FAILURE);
                }

                if(buffer[1] != 'M' && buffer[1] != 'S' && buffer[1] != 'F' && buffer[1] != 'W')//Unknown ingredient
                {
                        if(close(fd) == -1)
                        {
                                print("closing input error: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno),__LINE__);
                                _exit(EXIT_FAILURE);
                        }
                        print("Unknown ingredient, line: %d\n", STDERR_FILENO, 1, lineNum+1);
                        _exit(EXIT_FAILURE);
                }
                lineNum++;
        }

        if(lineNum < 10)
        {
                if(close(fd) == -1)
                {
                        print("closing input error: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno),__LINE__);
                        _exit(EXIT_FAILURE);
                }
                print("There must be at least 10 deliver in file, line: %d\n", STDERR_FILENO, 1, __LINE__);
                _exit(EXIT_FAILURE);
        }

        /**************************************************************************************/

        /*****************CREATE INGREDIENT SEMAPHORE*************************/
        if((semid = semget(IPC_PRIVATE, 4, S_IRUSR | S_IWUSR)) == -1)
        {
                print("semget error: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        arg.buf = &ds;
        if(semctl(semid, 0, IPC_STAT, arg) == -1)
        {
                print("semctl error: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        arg.array = calloc(ds.sem_nsems, sizeof(arg.array[0]));
        if(semctl(semid, 0, SETALL, arg) == -1)
        {
                print("semctl error: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        /***********************SEMAPHORE THAT INDICATES DESERT READYNESS**************************/
        if(sem_init(&gullacLock, 0, 0) == -1)
        {
                print("sem_init error: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        if(atexit(cleanUp) == -1) //In case of exit, clean resources and semaphores
        {
                print("registering atexit function error: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        /*****************************CREATING THREADS**************************************/
        //Send chef id's to thread so that can print correct output and wait for different ingredient
        status = pthread_create(&chef1, NULL, chefThread, &c1);
        if(status != 0)
        {
                print("pthread create chef1 error: %s, line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE);
        }

        status = pthread_create(&chef2, NULL, chefThread, &c2);
        if(status != 0)
        {
                print("pthread create chef2 error: %s, line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE);
        }

        status = pthread_create(&chef3, NULL, chefThread, &c3);
        if(status != 0)
        {
                print("pthread create chef3 error: %s, line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE);
        }

        status = pthread_create(&chef4, NULL, chefThread, &c4);
        if(status != 0)
        {
                print("pthread create chef4 error: %s, line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE);
        }

        status = pthread_create(&chef5, NULL, chefThread, &c5);
        if(status != 0)
        {
                print("pthread create chef5 error: %s, line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE);
        }

        status = pthread_create(&chef6, NULL, chefThread, &c6);
        if(status != 0)
        {
                print("pthread create chef6 error: %s, line: %d\n", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE);
        }
        /**********************************************************************************/

        /***********************DELIVER INGREDIENTS**********************************************/
        if(lseek(fd, 0, SEEK_SET) == -1)//Seek file to start at beginning
        {
                print("lseek error: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        opIngredient[0].sem_num = 0;
        opIngredient[0].sem_flg = 0;
        opIngredient[1].sem_num = 1;
        opIngredient[1].sem_flg = 0;
        opIngredient[2].sem_num = 2;
        opIngredient[2].sem_flg = 0;
        opIngredient[3].sem_num = 3;
        opIngredient[3].sem_flg = 0;
        /* MILK = 0 ----------- SUGAR = 1 ---------- FLOUR = 2 ---------- WALNUTS = 3 */
        //For every ingredient put different semaphore on set. For missing ones increase 2 times so only related chef can take.
        while((status = read(fd, ingredients, 3)) != 0 && status != -1)
        {
                if((ingredients[0] == 'M' && ingredients[1] == 'F') || (ingredients[0] == 'F' && ingredients[1] == 'M'))
                {
                        opIngredient[0].sem_op = 2;
                        opIngredient[1].sem_op = 1;
                        opIngredient[2].sem_op = 2;
                        opIngredient[3].sem_op = 1;
                        print("the wholesaler delivers milk and flour\n", STDOUT_FILENO, 0);
                        
                }
                else if((ingredients[0] == 'M' && ingredients[1] == 'W') || (ingredients[0] == 'W' && ingredients[1] == 'M'))
                {       
                        opIngredient[0].sem_op = 2;
                        opIngredient[1].sem_op = 1;
                        opIngredient[2].sem_op = 1;
                        opIngredient[3].sem_op = 2;
                        print("the wholesaler delivers milk and walnuts\n", STDOUT_FILENO, 0);
                }
                else if((ingredients[0] == 'M' && ingredients[1] == 'S') || (ingredients[0] == 'S' && ingredients[1] == 'M'))
                {       
                        opIngredient[0].sem_op = 2;
                        opIngredient[1].sem_op = 2;
                        opIngredient[2].sem_op = 1;
                        opIngredient[3].sem_op = 1;
                        print("the wholesaler delivers milk and sugar\n", STDOUT_FILENO, 0);
                }
                else if((ingredients[0] == 'F' && ingredients[1] == 'W') || (ingredients[0] == 'W' && ingredients[1] == 'F'))
                {       
                        opIngredient[0].sem_op = 1;
                        opIngredient[1].sem_op = 1;
                        opIngredient[2].sem_op = 2;
                        opIngredient[3].sem_op = 2;
                        print("the wholesaler delivers flour and walnuts\n", STDOUT_FILENO, 0);
                }
                else if((ingredients[0] == 'F' && ingredients[1] == 'S') || (ingredients[0] == 'S' && ingredients[1] == 'F'))
                {       
                        opIngredient[0].sem_op = 1;
                        opIngredient[1].sem_op = 2;
                        opIngredient[2].sem_op = 2;
                        opIngredient[3].sem_op = 1;
                        print("the wholesaler delivers flour and sugar\n", STDOUT_FILENO, 0);
                }
                else if((ingredients[0] == 'W' && ingredients[1] == 'S') || (ingredients[0] == 'S' && ingredients[1] == 'W'))
                {       
                        opIngredient[0].sem_op = 1;
                        opIngredient[1].sem_op = 2;
                        opIngredient[2].sem_op = 1;
                        opIngredient[3].sem_op = 2;
                        print("the wholesaler delivers walnut and sugar\n", STDOUT_FILENO, 0);
                }

                if(semop(semid, opIngredient, 4) == -1) //Give access to releated chef
                {
                        print("semop error at present ingredients: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                        exit(EXIT_FAILURE); 
                }
                print("the wholesaler waits for desert\n", STDOUT_FILENO, 0);
                
                if(sem_wait(&gullacLock) == -1) //wait for gullac
                {
                        print("sem_wait error at gullacLock: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                        exit(EXIT_FAILURE);
                }
                print("the wholesaler takes desert\n", STDOUT_FILENO, 0);
        }

        if(status == -1)
        {
                print("read error at input file: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE);
        }

        /************KILL ALL CHEFS*********************/
        opIngredient[0].sem_op = 2;//Increase all 2 times so any chef can access
        opIngredient[1].sem_op = 2;
        opIngredient[2].sem_op = 2;
        opIngredient[3].sem_op = 2;
        ingredients[0] = 'P'; //put poison on the sell list and kill all chefs
        if(semop(semid, opIngredient, 4) == -1)
        {
                print("semop error at present ingredients: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                exit(EXIT_FAILURE); 
        }
        /**********************************************/

        print("Wholesaler done suppplying...\n", STDOUT_FILENO, 0);

        /*********************************JOIN THREADS****************************************************/
        status = pthread_join(chef1, &returnStatus);
        if(status != 0)
        {
                print("pthread join chef1 error: %s, line: %d", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE);
        }
        returnStatusInt = (int*) returnStatus;
        print("Chef %d returned to main thread...\n", STDOUT_FILENO, 1, (*returnStatusInt));

        status = pthread_join(chef2, &returnStatus);
        if(status != 0)
        {
                print("pthread join chef2 error: %s, line: %d", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE);
        }
        returnStatusInt = (int*) returnStatus;
        print("Chef %d returned to main thread...\n", STDOUT_FILENO, 1, (*returnStatusInt));

        status = pthread_join(chef3, &returnStatus);
        if(status != 0)
        {
                print("pthread join chef3 error: %s, line: %d", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE);
        }
        returnStatusInt = (int*) returnStatus;
        print("Chef %d returned to main thread...\n", STDOUT_FILENO, 1, (*returnStatusInt));

        status = pthread_join(chef4, &returnStatus);
        if(status != 0)
        {
                print("pthread join chef4 error: %s, line: %d", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE);
        }
        returnStatusInt = (int*) returnStatus;
        print("Chef %d returned to main thread...\n", STDOUT_FILENO, 1, (*returnStatusInt));

        status = pthread_join(chef5, &returnStatus);
        if(status != 0)
        {
                print("pthread join chef5 error: %s, line: %d", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE);
        }
        returnStatusInt = (int*) returnStatus;
        print("Chef %d returned to main thread...\n", STDOUT_FILENO, 1, (*returnStatusInt));

        status = pthread_join(chef6, &returnStatus);
        if(status != 0)
        {
                print("pthread join chef6 error: %s, line: %d", STDERR_FILENO, 2, strerror(status), __LINE__);
                exit(EXIT_FAILURE);
        }
        returnStatusInt = (int*) returnStatus;
        print("Chef %d returned to main thread...\n", STDOUT_FILENO, 1, (*returnStatusInt));
        /************************************************************************************/

        exit(EXIT_SUCCESS);
}

int print(const char* str, int output, int argNum, ...) //Print function
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

void *chefThread(void *arg)
{
        int chefID = *((int*)arg);//take chef id to which thread chef we are in
        struct sembuf opChefs[4];

        //Match ingredients with semaphores on set
        opChefs[0].sem_num = 0;
        opChefs[0].sem_flg = 0;
        opChefs[1].sem_num = 1;
        opChefs[1].sem_flg = 0;
        opChefs[2].sem_num = 2;
        opChefs[2].sem_flg = 0;
        opChefs[3].sem_num = 3;
        opChefs[3].sem_flg = 0;

        while(1)
        {
                if(chefID == 1)
                {
                        print("chef1 is waiting for sugar and walnuts\n", STDOUT_FILENO, 0);
                        opChefs[0].sem_op = -1;
                        opChefs[1].sem_op = -2;
                        opChefs[2].sem_op = -1;
                        opChefs[3].sem_op = -2;

                        if(semop(semid, opChefs, 4) == -1) //take access to ingredients
                        {
                                print("semop error at taking ingredients: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE); 
                        }

                        if(ingredients[0] == 'P') //Chef takes poison and dies
                        {
                                //give back resources
                                opChefs[0].sem_op = 1;
                                opChefs[1].sem_op = 2;
                                opChefs[2].sem_op = 1;
                                opChefs[3].sem_op = 2;

                                if(semop(semid, opChefs, 4) == -1)
                                {
                                        print("semop error at taking ingredients: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE); 
                                }
                                break;
                        }

                        if((ingredients[0] == 'W' && ingredients[1] == 'S') || (ingredients[0] == 'S' && ingredients[1] == 'W'))//If there is missing ones take and prepare gullac
                        {
                                print("chef1 has taken the sugar\n", STDOUT_FILENO, 0);
                                print("chef1 has taken the walnuts\n", STDOUT_FILENO, 0);
                                print("chef1 is is preparing the dessert\n", STDOUT_FILENO, 0);
                                sleep((rand()%5) + 1);
                                print("chef1 has delivered the dessert to the wholesaler\n", STDOUT_FILENO, 0);
                                if(sem_post(&gullacLock) == -1) //gullac is ready, wholesaler can take it
                                {
                                        print("sem_post gullacLock error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);   
                                }
                        }
                        
                }
                else if(chefID == 2)
                {
                        print("chef2 is waiting for sugar and flour\n", STDOUT_FILENO, 0);
                        opChefs[0].sem_op = -1;
                        opChefs[1].sem_op = -2;
                        opChefs[2].sem_op = -2;
                        opChefs[3].sem_op = -1;
                        
                        if(semop(semid, opChefs, 4) == -1)//take access to ingredients
                        {
                                print("semop error at taking ingredients: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE); 
                        }

                        if(ingredients[0] == 'P') //Chef takes poison and dies
                        {
                                //give back resources
                                opChefs[0].sem_op = 1;
                                opChefs[1].sem_op = 2;
                                opChefs[2].sem_op = 2;
                                opChefs[3].sem_op = 1;

                                if(semop(semid, opChefs, 4) == -1)
                                {
                                        print("semop error at taking ingredients: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE); 
                                }
                                break;
                        }
        
                        if((ingredients[0] == 'F' && ingredients[1] == 'S') || (ingredients[0] == 'S' && ingredients[1] == 'F'))//If there is missing ones take and prepare gullac
                        {
                                print("chef2 has taken the sugar\n", STDOUT_FILENO, 0);
                                print("chef2 has taken the flour\n", STDOUT_FILENO, 0);
                                print("chef2 is is preparing the dessert\n", STDOUT_FILENO, 0);
                                sleep((rand()%5) + 1);
                                print("chef2 has delivered the dessert to the wholesaler\n", STDOUT_FILENO, 0);
                                if(sem_post(&gullacLock) == -1)//gullac is ready, wholesaler can take it
                                {
                                        print("sem_post gullacLock error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);   
                                }
                        }
                }
                else if(chefID == 3)
                {
                        print("chef3 is waiting for flour and walnuts\n", STDOUT_FILENO, 0);
                        opChefs[0].sem_op = -1;
                        opChefs[1].sem_op = -1;
                        opChefs[2].sem_op = -2;
                        opChefs[3].sem_op = -2;

                        if(semop(semid, opChefs, 4) == -1)//take access to ingredients
                        {
                                print("semop error at taking ingredients: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE); 
                        }

                        if(ingredients[0] == 'P') //Chef takes poison and dies
                        {
                                //give back resources
                                opChefs[0].sem_op = 1;
                                opChefs[1].sem_op = 1;
                                opChefs[2].sem_op = 2;
                                opChefs[3].sem_op = 2;

                                if(semop(semid, opChefs, 4) == -1)
                                {
                                        print("semop error at taking ingredients: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE); 
                                }
                                break;
                        }

                        if((ingredients[0] == 'F' && ingredients[1] == 'W') || (ingredients[0] == 'W' && ingredients[1] == 'F'))//If there is missing ones take and prepare gullac
                        {
                                print("chef3 has taken the flour\n", STDOUT_FILENO, 0);
                                print("chef3 has taken the walnuts\n", STDOUT_FILENO, 0);
                                print("chef3 is is preparing the dessert\n", STDOUT_FILENO, 0);
                                sleep((rand()%5) + 1);
                                print("chef3 has delivered the dessert to the wholesaler\n", STDOUT_FILENO, 0);
                                if(sem_post(&gullacLock) == -1)//gullac is ready, wholesaler can take it
                                {
                                        print("sem_post gullacLock error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);   
                                }
                        }
                }
                else if(chefID == 4)
                {
                        print("chef4 is waiting for sugar and milk\n", STDOUT_FILENO, 0);
                        opChefs[0].sem_op = -2;
                        opChefs[1].sem_op = -2;
                        opChefs[2].sem_op = -1;
                        opChefs[3].sem_op = -1;

                        if(semop(semid, opChefs, 4) == -1)//take access to ingredients
                        {
                                print("semop error at taking ingredients: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE); 
                        }

                        if(ingredients[0] == 'P') //Chef takes poison and dies
                        {
                                //give back resources
                                opChefs[0].sem_op = 2;
                                opChefs[1].sem_op = 2;
                                opChefs[2].sem_op = 1;
                                opChefs[3].sem_op = 1;

                                if(semop(semid, opChefs, 4) == -1)
                                {
                                        print("semop error at taking ingredients: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE); 
                                }
                                break;
                        }

                        if((ingredients[0] == 'M' && ingredients[1] == 'S') || (ingredients[0] == 'S' && ingredients[1] == 'M'))//If there is missing ones take and prepare gullac
                        {
                                print("chef4 has taken the sugar\n", STDOUT_FILENO, 0);
                                print("chef4 has taken the milk\n", STDOUT_FILENO, 0);
                                print("chef4 is is preparing the dessert\n", STDOUT_FILENO, 0);
                                sleep((rand()%5) + 1);
                                print("chef4 has delivered the dessert to the wholesaler\n", STDOUT_FILENO, 0);
                                if(sem_post(&gullacLock) == -1)//gullac is ready, wholesaler can take it
                                {
                                        print("sem_post gullacLock error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);   
                                }
                        }
                }
                else if(chefID == 5)
                {
                        print("chef5 is waiting for milk and walnuts\n", STDOUT_FILENO, 0);
                        opChefs[0].sem_op = -2;
                        opChefs[1].sem_op = -1;
                        opChefs[2].sem_op = -1;
                        opChefs[3].sem_op = -2;
                        
                        if(semop(semid, opChefs, 4) == -1)//take access to ingredients
                        {
                                print("semop error at taking ingredients: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE); 
                        }

                        if(ingredients[0] == 'P') //Chef takes poison and dies
                        {
                                //give back resources
                                opChefs[0].sem_op = 2;
                                opChefs[1].sem_op = 1;
                                opChefs[2].sem_op = 1;
                                opChefs[3].sem_op = 2;

                                if(semop(semid, opChefs, 4) == -1)
                                {
                                        print("semop error at taking ingredients: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE); 
                                }
                                break;
                        }

                        if((ingredients[0] == 'M' && ingredients[1] == 'W') || (ingredients[0] == 'W' && ingredients[1] == 'M'))//If there is missing ones take and prepare gullac
                        {
                                print("chef5 has taken the milk\n", STDOUT_FILENO, 0);
                                print("chef5 has taken the walnuts\n", STDOUT_FILENO, 0);
                                print("chef5 is is preparing the dessert\n", STDOUT_FILENO, 0);
                                sleep((rand()%5) + 1);
                                print("chef5 has delivered the dessert to the wholesaler\n", STDOUT_FILENO, 0);
                                if(sem_post(&gullacLock) == -1)//gullac is ready, wholesaler can take it
                                {
                                        print("sem_post gullacLock error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);   
                                }
                        }

                }
                else if(chefID == 6)
                {
                        print("chef6 is waiting for milk and flour\n", STDOUT_FILENO, 0);
                        
                        opChefs[0].sem_op = -2;
                        opChefs[1].sem_op = -1;
                        opChefs[2].sem_op = -2;
                        opChefs[3].sem_op = -1;
                        
                        if(semop(semid, opChefs, 4) == -1)//take access to ingredients
                        {
                                print("semop error at taking ingredients: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                exit(EXIT_FAILURE); 
                        }

                        if(ingredients[0] == 'P') //Chef takes poison and dies
                        {
                                //give back resources
                                opChefs[0].sem_op = 2;
                                opChefs[1].sem_op = 1;
                                opChefs[2].sem_op = 2;
                                opChefs[3].sem_op = 1;

                                if(semop(semid, opChefs, 4) == -1)
                                {
                                        print("semop error at taking ingredients: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE); 
                                }
                                break;
                        }

                        if((ingredients[0] == 'M' && ingredients[1] == 'F') || (ingredients[0] == 'F' && ingredients[1] == 'M'))//If there is missing ones take and prepare gullac
                        {
                                print("chef6 has taken the milk\n", STDOUT_FILENO, 0);
                                print("chef6 has taken the flour\n", STDOUT_FILENO, 0);
                                print("chef6 is is preparing the dessert\n", STDOUT_FILENO, 0);
                                sleep((rand()%5) + 1);
                                print("chef6 has delivered the dessert to the wholesaler\n", STDOUT_FILENO, 0);
                                if(sem_post(&gullacLock) == -1)//gullac is ready, wholesaler can take it
                                {
                                        print("sem_post gullacLock error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                                        exit(EXIT_FAILURE);   
                                }
                               
                        }
                        
                }
        }
        return arg;
}

void cleanUp()
{
        if(close(fd) == -1) //Close input file
        {
                print("closing input file error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                _exit(EXIT_FAILURE);
        }
        if(sem_destroy(&gullacLock) == -1)//destory gullacLock POSIX semaphore
        {
                print("sem_destroy gullacLock error: %s, line: %d", STDERR_FILENO, 2, strerror(errno), __LINE__);
                _exit(EXIT_FAILURE);
        }
        if(semctl(semid, 0, IPC_RMID, NULL) == -1)//destroy ingredient systemV semaphore
        {
                print("semctl error: %s, line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                _exit(EXIT_FAILURE);
        }
}