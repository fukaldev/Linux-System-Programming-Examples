#include "helper.h"

#define BUFFER_SIZE 120
#define METRIC_SIZE 100

sig_atomic_t existence = 1;
char* input; 

int main(int argc, char *argv[])
{
        struct sigaction sigact;
        sigact.sa_flags = SA_RESTART;
        sigemptyset(&sigact.sa_mask);
        char template[32] = "tempXXXXXX";
        int tempFd = mkstemp(template);
        unlink(template);
        int status;
        char line[BUFFER_SIZE];
        char* output;
        int opt;
        int err = 0;
        
        while ((opt = getopt(argc, argv, ":i:o:")) != -1)
        {
                switch (opt)
                {
                case 'i':
                        input = optarg;
                        break;
                case 'o':
                        output = optarg;
                        break;
                case '?':
                        fprintf(stderr,"Unknown option: -%c\n", optopt);
                        err++;
                        break;
                case ':':
                        fprintf(stderr,"Missing arg for -%c\n", optopt);
                        err++;
                        break;
                }
        }

        if(err)
        {
                fprintf(stderr,"Usage: -f [inputPath] -o [outputPath]\n");
                _exit(EXIT_FAILURE);
        }

        int inputFd;
        inputFd = open(input, O_RDONLY, S_IRUSR);
        if (inputFd < 0)
        {
                fprintf(stderr, "can't opened input file: %s\n", strerror(errno));
                _exit(EXIT_FAILURE);
        }

        int outputFd;
        outputFd = open(output, O_WRONLY | O_CREAT,0666);
        if (outputFd < 0)
        {
                fprintf(stderr, "can't opened output file: %s\n", strerror(errno));
                _exit(EXIT_FAILURE);
        }

        /*******SIGNAL HANDLERS***************/
        sigact.sa_handler = termProcess;
        if (sigaction(SIGTERM, &sigact, NULL) < 0)
        {
                fprintf(stderr, "sigterm handler setting error: %s\n", strerror(errno));
                _exit(EXIT_FAILURE);
        }

        sigact.sa_handler = generalHandler;
        if (sigaction(SIGINT, &sigact, NULL) < 0)
        {
                fprintf(stderr, "sigint handling setting error: %s\n", strerror(errno));
                _exit(EXIT_FAILURE);
        }

        // sigact.sa_handler = generalHandler;
        // if (sigaction(SIGSTOP, &sigact, NULL) < 0)
        // {
        //         fprintf(stderr, "sigstop handling setting error error: %s\n", strerror(errno));
        //         _exit(EXIT_FAILURE);
        // }

        sigact.sa_handler = justReturn;
        if(sigaction(SIGALRM, &sigact, NULL) < 0)
        {
                fprintf(stderr, "sigalrm signal error: %s\n", strerror( errno ));
                _exit(EXIT_FAILURE);
        }

        sigact.sa_handler = p1Done;
        if(sigaction(SIGUSR1, &sigact, NULL) < 0)
        {
                fprintf(stderr, "sigusr1 signal error: %s\n", strerror( errno ));
                _exit(EXIT_FAILURE);
        }

        /*****************BLOCKING SIGNALS FOR P2******************/
        sigset_t oldMask, newMask, waitMask, pendMask; //Signal sets
        sigemptyset(&oldMask);
        sigaddset(&oldMask, SIGALRM);//Block SIGALRM to start
        sigaddset(&oldMask, SIGUSR1);//Block SIGALRM to start
        if (sigprocmask(SIG_SETMASK, &oldMask, NULL) < 0)//Set block at beginning so P2 can set it's wait and pending methods, after releasing lock, signals will delivered
        {
                fprintf(stderr, "sigsuspend error: %s\n", strerror(errno));
                _exit(EXIT_FAILURE);
        }
        
        /*********lock struct*****/
        struct flock lock;
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = 0;
        /*************************/

        pid_t process = fork();//Create new process

        if (process == -1)
        {
                fprintf(stderr, "Error forking: %s\n", strerror(errno));
                _exit(EXIT_FAILURE);
        }

        /***************************************P2***************************************************/
        if(process == 0)
        {
                int numRead;
                int i, j = 0, bufferRate = 1;
                char writeBuffer[BUFFER_SIZE];
                char errorBuffer[BUFFER_SIZE];

                double maeMean;
                double maeDev;
                double mseMean;
                double mseDev;
                double rmseMean;
                double rmseDev;


                double* MAE = NULL;
                double* MSE = NULL;
                double* RMSE = NULL;
                
                MAE = (double*) calloc(METRIC_SIZE, sizeof(double));
                MSE = (double*) calloc(METRIC_SIZE, sizeof(double));
                RMSE = (double*) calloc(METRIC_SIZE, sizeof(double));

                if(!MAE || !MSE || !RMSE)
                {
                        fprintf(stderr, "metric array couldn't allocated error: %s\n", strerror(errno));
                        _exit(EXIT_FAILURE);
                }

                metrics m;

                sigfillset(&waitMask);
                sigdelset(&waitMask, SIGALRM);//While suspending for new input, if there is signal SIGALRM or SIGTERM continue
                sigdelset(&waitMask, SIGTERM);

                sigemptyset(&newMask);
                sigaddset(&newMask, SIGINT);//Block mask for blocking critical region
                sigaddset(&newMask, SIGSTOP);
                
                if(sigsuspend(&waitMask) != -1)//Suspend until temp file filled
                {
                        fprintf(stderr, "sigsuspend until temp available error: %s\n", strerror( errno ));
                        _exit(EXIT_FAILURE);
                }

                while (1)
                {
                        errorBuffer[0] = 0;
                        if(sigpending(&pendMask) < 0)//Look, SIGUSR1 arrived. If it arrived this means P1 is done.
                        {
                                fprintf(stderr, "sigpending SIGUSR1 error: %s\n", strerror(errno));
                                _exit(EXIT_FAILURE); 
                        }
                        if(sigismember(&pendMask, SIGUSR1))//Look SIGUSR1 is pending
                                existence = 0;
                
                        lock.l_type = F_WRLCK; //Write lock also blocks reads, one process can access temp at a time
                        if (fcntl(tempFd, F_SETLKW, &lock) < 0)//Lock for reading by writing. Writing also locks reading
                        {
                                fprintf(stderr, "lock p2 error: %s\n", strerror(errno));
                                _exit(EXIT_FAILURE);
                        }

                        if (lseek(tempFd, 0, SEEK_SET) < 0)//Set seek to beginning of file to read always first line
                        {
                                fprintf(stderr, "seek1 error: %s\n", strerror(errno));
                                _exit(EXIT_FAILURE);
                        }

                        status = read(tempFd, line, BUFFER_SIZE);
                        
                        if (status < 0)
                        {
                                fprintf(stderr, "read tempFd error: %s\n", strerror(errno));
                                _exit(EXIT_FAILURE);
                        }
                        
                        if (existence == 0 && status == 0)//If there is no line and process1 terminated, there is no input in temp file.Exit.
                        {
                                break;
                        }

                        if (existence == 1 && status == 0) //There will be input that are processing, so wait
                        {
                                lock.l_type = F_UNLCK;
                                if (fcntl(tempFd, F_SETLK, &lock) < 0)
                                {
                                        fprintf(stderr, "lock error: %s\n", strerror(errno));
                                        _exit(EXIT_FAILURE);
                                }
                                if (sigsuspend(&waitMask) != -1)
                                {
                                        fprintf(stderr, "sigsuspend error: %s\n", strerror(errno));
                                        _exit(EXIT_FAILURE);
                                }
                                continue;
                        }
                        for (i = 0; i < BUFFER_SIZE; i++)
                                if (line[i] == '\n')
                                        break;
                        if (lseek(tempFd, (-(status - i))+1, SEEK_CUR) < 0)//Seek at the end of readed line, so when deleting readed line start from next line
                        {
                                fprintf(stderr, "seek2 error: %s\n", strerror(errno));
                                _exit(EXIT_FAILURE);
                        }  

                        if(deleteFirstLine(tempFd) == -1)//Delete first line that readed
                        {
                                fprintf(stderr, "deleting line error: %s\n", strerror(errno));
                                _exit(EXIT_FAILURE); 
                        }


                        lock.l_type = F_UNLCK;
                        if (fcntl(tempFd, F_SETLK, &lock) < 0)//Unlock file for process 1 to continue
                        {
                                fprintf(stderr, "unlock p2 error: %s\n", strerror(errno));
                                _exit(EXIT_FAILURE);
                        }
                        if (sigprocmask(SIG_BLOCK, &newMask, &oldMask) < 0)//Start critical region so SIGALRM or SIGINT or SIGSTP can not interrupt calculation
                        { 
                                fprintf(stderr, "sigprocmask p2 error: %s\n", strerror(errno));
                                _exit(EXIT_FAILURE);
                        }
                        processLine(line, errorBuffer, &m);
                        line[i] = '\0';
                        strcat(line, errorBuffer);
                        
                        if (sigprocmask(SIG_SETMASK, &oldMask, NULL) < 0)
                        {
                                fprintf(stderr, "sigprocmask p2 error: %s\n", strerror(errno));
                                _exit(EXIT_FAILURE);
                        }
                        status = write(outputFd, line, strlen(line));

                        if (status != strlen(line))
                        {

                                fprintf(stderr, "write error3: %s\n", strerror(errno));
                                _exit(EXIT_FAILURE);
                        }

                        if (status < 0)
                        {

                                fprintf(stderr, "write error4: %s\n", strerror(errno));
                                _exit(EXIT_FAILURE);
                        }
                        MAE[j] = m.mae;
                        MSE[j] = m.mse;
                        RMSE[j] = m.rmse;
                        j++;
                        if(j == METRIC_SIZE*bufferRate)
                        {
                                MAE = (double*) realloc(MAE, 100);
                                MSE = (double*) realloc(MSE, 100);
                                RMSE = (double*)realloc(RMSE, 100);
                                if(!MAE || !MSE || !RMSE)
                                {
                                        fprintf(stderr, "metric array reallocation error: %s\n", strerror(errno));
                                        _exit(EXIT_FAILURE);
                                }
                                bufferRate++;
                        } 
                }
                calculateMetric(MAE, &maeMean, &maeDev, j);
                calculateMetric(MSE, &mseMean, &mseDev, j);
                calculateMetric(RMSE, &rmseMean, &rmseDev, j);

                printf("Mean of MAE: %.3f, Standart Deviation of MAE: %.3f\n", maeMean, maeDev);
                printf("Mean of MSE: %.3f, Standart Deviation of MSE: %.3f\n", mseMean, mseDev);
                printf("Mean of RMSE: %.3f, Standart Deviation of RMSE: %.3f\n", rmseMean, rmseDev);

                status = remove(input);
                if (status < 0)
                {
                        fprintf(stderr, "remove 4: %s\n", strerror(errno));
                        _exit(EXIT_FAILURE);
                }
                _exit(EXIT_SUCCESS);
        }
        /********************************************************************************************/

        /***************************************P1***************************************************/
        else
        {
                printf("P1 PID: %d, P2 PID: %d\n", getpid(), process);
                char buffer[20];
                int totalLine = 0, totalByte = 0;
                int interrupt = 0, stop = 0;

                sigemptyset(&pendMask);
                sigemptyset(&newMask);
                sigemptyset(&oldMask);
                sigaddset(&newMask, SIGINT);
                sigaddset(&newMask, SIGSTOP);
                while(1)
                {
                        line[0] = 0; //Start from beginning of array to put characters
                        status = read(inputFd, buffer, 20);
                        if(status < 0)
                        {
                                fprintf(stderr, "read input error: %s\n", strerror( errno ));
                                _exit(EXIT_FAILURE);   
                        }
                        if(status < 20)
                                break;
                        totalByte += status;
                        

                        if(sigprocmask(SIG_BLOCK, &newMask, &oldMask) < 0)
                        {//Save old mask and block SIGINT and SIGSTOP
                                fprintf(stderr, "sigprocmask process1 critical region begin error: %s\n", strerror( errno ));
                                _exit(EXIT_FAILURE);   
                        }

                        calculateAndCreateLine(&buffer[0], &line[0]);

                        if(sigpending(&pendMask) < 0)
                        {
                                fprintf(stderr, "sigpending process1 error: %s\n", strerror( errno ));
                                _exit(EXIT_FAILURE);
                        }
                        if (sigismember(&pendMask, SIGSTOP))
                                stop++;
                        if (sigismember(&pendMask, SIGINT))
                                interrupt++;

                        if(sigprocmask(SIG_SETMASK, &oldMask, NULL) < 0)
                        {
                                fprintf(stderr, "sigprocmask process1 critical region end error: %s\n", strerror( errno ));
                                _exit(EXIT_FAILURE);
                        }


                        totalLine++;

                        lock.l_type = F_WRLCK;
                        if(fcntl(tempFd, F_SETLKW, &lock) < 0)
                        {
                                fprintf(stderr, "process1 set lock error: %s\n", strerror( errno ));
                                _exit(EXIT_FAILURE);
                        }

                        if(lseek(tempFd, 0, SEEK_END) < 0)
                        {
                                fprintf(stderr, "adding end of file seek error: %s\n", strerror( errno ));
                                _exit(EXIT_FAILURE);
                        }
                        
                        status = write(tempFd, line, strlen(line));

                        if(status != strlen(line))
                        {
                                fprintf(stderr, "write tempFd error process1: %s\n", strerror( errno ));
                                _exit(EXIT_FAILURE);
                        }

                        lock.l_type = F_UNLCK;
                        if(fcntl(tempFd, F_SETLK, &lock) < 0)
                        {
                                fprintf(stderr, "process1 unlock error: %s\n", strerror( errno ));
                                _exit(EXIT_FAILURE);
                        }
                        if(kill(process, SIGALRM) < 0)//Tell process 2, P1 filled temp, if p2 need
                        {
                                fprintf(stderr, "send buffer filled signal error: %s\n", strerror( errno ));
                                _exit(EXIT_FAILURE);
                        }
                }

                printf("Total byte readed: %d\n", totalByte);
                printf("Total line equation: %d\n", totalLine);
                if(stop > 0)
                        printf("SIGSTOP delivered\n");
                if(interrupt > 0)
                        printf("SIGINT delivered\n");
                
                pid_t child;
                do 
                {
                        if(kill(process, SIGUSR1) < 0)//Send constantly P1 is done, by doing that P2 can catch this signal
                        {
                                fprintf(stderr, "sending termination P1 error: %s\n", strerror( errno ));
                                _exit(EXIT_FAILURE);
                        }
                        child = waitpid(process, &status, WNOHANG);
                } while(child != process);
                printf("Process Done!!!\n");
                _exit(EXIT_SUCCESS);
        }
}