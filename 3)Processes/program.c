#include "helper.h"

static volatile int liveChild = 0;

static void* pointers[20];
static size_t pointerNum = 0;

static int files[20];
static size_t fileNum = 0;

pid_t p2, p3, p4, p5;//PID of child processes

void addPointer(void* p) //Add pointer to array to cleanup after exit
{
        pointers[pointerNum++] = p;
}

void addFile(int fd) //Add file descriptor to array to cleanup after exit
{
        files[fileNum++] = fd;
}

void cleanUp() //Free all resources and close all files
{
        int status;
        size_t count;
        for(count = 0; count < pointerNum; count++)
                free(pointers[count]);
        
        for(count = 0; count < fileNum; count++)
                status = 0;
                if((status = close(files[count])) < 0 && status != EBADF)
                {
                        fprintf(stderr, "closing cleanup error: %s", strerror(errno));
                        _exit(EXIT_FAILURE);
                }
}

//SIGINT handler for parent. Parent catches and look for childs if they exist still. If they are still exist, sends SIGTERM to them and
//kills them. After that waits for childs and reap all childs.
void termParent(int signo) 
{
        int status;
        pid_t child;

        fprintf(stdout, "Parent Catched\n");
        if(kill(p2, 0) == 0)
        {
                if(kill(p2, SIGTERM) < 0)
                {
                        fprintf(stderr, "sigterm handler error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
        }
        if(kill(p3, 0) == 0)
        {
                if(kill(p3, SIGTERM) < 0)
                {
                        fprintf(stderr, "sigterm handler error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
        }
        if(kill(p4, 0) == 0)
        {
                if(kill(p4, SIGTERM) < 0)
                {
                        fprintf(stderr, "sigterm handler error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
        }
        if(kill(p5, 0) == 0)
        {
                if(kill(p5, SIGTERM) < 0)
                {
                        fprintf(stderr, "sigterm handler error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
        }

        while((child = waitpid(-1, &status, WNOHANG)) > 0)
                liveChild--;
        if(child == -1 && errno != ECHILD)
        {
                fprintf(stderr, "waitpid handler error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        exit(errno);
}

//When child gets signal of SIGINT it handles it and send it to Parent, so parent can start exiting process
void sendChild(int signo)
{
        write(STDOUT_FILENO, "Child Catched\n", 15);
        kill(getppid(), SIGINT);
}

//When childs get signal of SIGTERM they exit gracefully and cleanUp handles all release process 
void termChild(int signo)
{
        exit(EXIT_FAILURE);
}

//This is SIGCHILD handler. This handler works when parent receives SIGCHILD signal.
//After that it wait for all childs and reap them. By blocking SIGCHLD at parent
//we can wait synchronous a by suspending parent.
void childHandler(int signo)
{
        int status, savedErr;
        pid_t child;

        savedErr = errno;

        while((child = waitpid(-1, &status, WNOHANG)) > 0)
                liveChild--;
        
        if(child == -1 && errno != ECHILD)
                fprintf(stderr, "waitpid handler error: %s", strerror(errno));
        
        errno = savedErr;

}

int main(int argc, char *argv[])
{
        int inputA, inputB, status, numTemp;
        char* argumentA;  //Holds inputA file name
        char* argumentB; //Holds inputB file name
        int argumentN;  //n
        int opt;
        int err = 0;

        setbuf(stdout, NULL); //Disable buffering
        
        while ((opt = getopt(argc, argv, ":i:j:n:")) != -1)
        {
                switch (opt)
                {
                case 'i':
                        argumentA = optarg;
                        break;
                case 'j':
                        argumentB = optarg;
                        break;
                case 'n':
                        argumentN = strtol(optarg, NULL, 10);
                        if(argumentN < 0)
                                err++;
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
                fprintf(stderr,"Usage: -i [inputPathA] -j [inputPathB] -n [positiveInteger]\n");
                _exit(EXIT_FAILURE);
        }

        if(atexit(cleanUp) < 0)//Register cleanUp function. This function clear all resources and files in any case of exit() calling.
        {
                fprintf(stderr,"atexit register error: %s", strerror(errno));
                _exit(EXIT_FAILURE);
        }

        int dimension = pow(2, argumentN); //Indicates one dimension of matrix

        int barrier[2]; //To create syncronization barrier we use pipe paradigm.
        
        //For bidirectional pipe structure, we create 2 pipe for every process.
        /***********OPEN PIPES IN AND OUT****************************/
        int p2Pipe[2];
        int p3Pipe[2];
        int p4Pipe[2];
        int p5Pipe[2];
        int p2PipeRev[2];
        int p3PipeRev[2];
        int p4PipeRev[2];
        int p5PipeRev[2];

        if(pipe(barrier) < 0)
        {
                fprintf(stderr, "pipe p2 error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        addFile(barrier[0]);
        addFile(barrier[1]);

        if(pipe(p2Pipe) < 0)
        {
                fprintf(stderr, "pipe p2 error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        addFile(p2Pipe[0]);
        addFile(p2Pipe[1]);

        if(pipe(p3Pipe) < 0)
        {
                fprintf(stderr, "pipe p3 error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        addFile(p3Pipe[0]);
        addFile(p3Pipe[1]);

        if(pipe(p4Pipe) < 0)
        {
                fprintf(stderr, "pipe p4 error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        addFile(p4Pipe[0]);
        addFile(p4Pipe[1]);

        if(pipe(p5Pipe) < 0)
        {
                fprintf(stderr, "pipe p5 error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        addFile(p5Pipe[0]);
        addFile(p5Pipe[1]);

        if(pipe(p2PipeRev) < 0)
        {
                fprintf(stderr, "pipe p2Rev error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        addFile(p2PipeRev[0]);
        addFile(p2PipeRev[1]);

        if(pipe(p3PipeRev) < 0)
        {
                fprintf(stderr, "pipe p3Rev error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        addFile(p3PipeRev[0]);
        addFile(p3PipeRev[1]);

        if(pipe(p4PipeRev) < 0)
        {
                fprintf(stderr, "pipe p4Rev error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        addFile(p4PipeRev[0]);
        addFile(p4PipeRev[1]);

        if(pipe(p5PipeRev) < 0)
        {
                fprintf(stderr, "pipe p5Rev error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        addFile(p5PipeRev[0]);
        addFile(p5PipeRev[1]);
        /****************************************************************/

        sigset_t blockMask, emptyMask, intBlock;
        struct sigaction sa;

        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sa.sa_handler = childHandler;           //Set childHandler for SIGCHLD handler
        if(sigaction(SIGCHLD, &sa, NULL) == -1)
        {
                fprintf(stderr, "sigaction SIGCHLD handler: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }

        sigemptyset(&blockMask);
        sigaddset(&blockMask, SIGCHLD); //Block SIGCHLD for not to lost and get it at the end of parent
        if(sigprocmask(SIG_SETMASK, &blockMask, NULL) == -1)
        {
                fprintf(stderr, "block SIGCHLD handler: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        
        sigemptyset(&intBlock);
        sigaddset(&intBlock, SIGINT);
        sigaddset(&intBlock, SIGTERM);//Also block SIGINT and SIGTERM before forking and not to lost signal.
        if(sigprocmask(SIG_BLOCK, &intBlock, &blockMask) == -1)
        {
                fprintf(stderr, "block SIGINT handler: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }

        sa.sa_handler = termParent; //Set termParent for SIGINT handler
        if(sigaction(SIGINT, &sa, NULL) == -1)
        {
                fprintf(stderr, "sigaction SIGINT handler: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }

        liveChild = 4;  //We create 4 live child to be used and reaped after process is done

        /***********************OPEN INPUT FILES*************************************/
        if((inputA = open(argumentA, O_RDONLY, 0666)) < 0)
        {
                fprintf(stderr, "error on opening inputA file: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }

        if(strcmp(argumentA, argumentB) != 0) //If same input file name given as parameter don't try to open again
        {        if((inputB = open(argumentB, O_RDONLY, 0666)) < 0)
                {
                        fprintf(stderr, "error on opening inputB file: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
        }

        char* matrixBufferA = (char*) malloc(sizeof(char)*dimension*dimension);//Reading array for inputA
        if(!matrixBufferA)
        {
                fprintf(stderr, "malloc matrixBufferA error: %s", strerror(errno));
                exit(EXIT_FAILURE);  
        }
        addPointer(matrixBufferA);

        char* matrixBufferB = (char*) malloc(sizeof(char)*dimension*dimension);//Reading array for inputA
        if(!matrixBufferB)
        {
                fprintf(stderr, "malloc matrixBufferB error: %s", strerror(errno));
                exit(EXIT_FAILURE);  
        }
        addPointer(matrixBufferB);

        if((status = read(inputA, matrixBufferA, dimension*dimension)) < 0)//Read dimension*dimension char
        {
                fprintf(stderr, "error on reading inputA file: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        if(status < dimension*dimension)
        {
                fprintf(stderr, "not enough character inputA file: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        if(strcmp(argumentA, argumentB) == 0) //If they are same file then seek to begining of file and give same file descriptor to inputB
        {
                if((inputB = dup(inputA)) < 0) //Copy file descriptor input files are same
                {
                        fprintf(stderr, "dup parent process error: %s", strerror(errno));
                        exit(EXIT_FAILURE); 
                }
                if(lseek(inputB, 0, SEEK_SET) < 0) //Come back to beginnig of same file
                {
                        fprintf(stderr, "lseek parent process error: %s", strerror(errno));
                        exit(EXIT_FAILURE); 
                }
        }
        if((status = read(inputB, matrixBufferB, dimension*dimension)) < 0)//Read dimension*dimension char
        {
                fprintf(stderr, "error on reading inputB file: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        if(status < dimension*dimension)
        {
                fprintf(stderr, "not enough character inputB file: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }

        if(close(inputA) < 0)//inputA closed
        {
                fprintf(stderr, "inputA close error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }

        if(close(inputB) < 0)//inputB closed
        {
                fprintf(stderr, "inputB close error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }  

        /**************** PRINT MATRIX A and B *****************************/
        fprintf(stdout, "**************MATRIX A**************\n");

        for(int i = 0; i < dimension; i++)
        {
                for(int j = 0; j < dimension; j++)
                {
                        if(j%dimension == 0)
                                fprintf(stdout, "\n");
                        fprintf(stdout, " %5d ", matrixBufferA[(dimension*i)+j]);
                }
        }

        
        fprintf(stdout, "\n\n\n");

        
        fprintf(stdout, "**************MATRIX B**************\n");
        for(int i = 0; i < dimension; i++)
        {
                for(int j = 0; j < dimension; j++)
                {
                        if(j%dimension == 0)
                                fprintf(stdout, "\n");
                        fprintf(stdout, " %5d ", matrixBufferB[(dimension*i)+j]);
                }
        }

        

        fprintf(stdout, "\n\n\n");

        /********************************************************************/

        /********************CREATE QUARTERS OF MATRIX A and B************************/
        char* matrixA11 = (char*) malloc(sizeof(char)*(dimension/2)*(dimension/2));
        if(!matrixA11)
        {
                fprintf(stderr, "malloc matrixA11 error: %s", strerror(errno));
                exit(EXIT_FAILURE);  
        }
        addPointer(matrixA11);

        char* matrixA12 = (char*) malloc(sizeof(char)*(dimension/2)*(dimension/2));
        if(!matrixA12)
        {
                fprintf(stderr, "malloc matrixA12 error: %s", strerror(errno));
                exit(EXIT_FAILURE);  
        }
        addPointer(matrixA12);

        char* matrixA21 = (char*) malloc(sizeof(char)*(dimension/2)*(dimension/2));
        if(!matrixA21)
        {
                fprintf(stderr, "malloc matrixA21 error: %s", strerror(errno));
                exit(EXIT_FAILURE);  
        }
        addPointer(matrixA21);

        char* matrixA22 = (char*) malloc(sizeof(char)*(dimension/2)*(dimension/2));
        if(!matrixA22)
        {
                fprintf(stderr, "malloc matrixA22 error: %s", strerror(errno));
                exit(EXIT_FAILURE);  
        }
        addPointer(matrixA22);

        char* matrixB11 = (char*) malloc(sizeof(char)*(dimension/2)*(dimension/2));
        if(!matrixB11)
        {
                fprintf(stderr, "malloc matrixB11 error: %s", strerror(errno));
                exit(EXIT_FAILURE);  
        }
        addPointer(matrixB11);

        char* matrixB12 = (char*) malloc(sizeof(char)*(dimension/2)*(dimension/2));
        if(!matrixB12)
        {
                fprintf(stderr, "malloc matrixB12 error: %s", strerror(errno));
                exit(EXIT_FAILURE);  
        }
        addPointer(matrixB12);

        char* matrixB21 = (char*) malloc(sizeof(char)*(dimension/2)*(dimension/2));
        if(!matrixB21)
        {
                fprintf(stderr, "malloc matrixB21 error: %s", strerror(errno));
                exit(EXIT_FAILURE);  
        }
        addPointer(matrixB21);

        char* matrixB22 = (char*) malloc(sizeof(char)*(dimension/2)*(dimension/2));
        if(!matrixB22)
        {
                fprintf(stderr, "malloc matrixB22 error: %s", strerror(errno));
                exit(EXIT_FAILURE);  
        }
        addPointer(matrixB22);

        /********************************DIVIDE MATRIX A and B INTO QUARTERS********************************/
        for(int i = 0; i < dimension/2; i++)
        {
                memcpy(matrixA11+(i*(dimension/2)),matrixBufferA+(dimension*i), sizeof(char)*dimension/2);
                memcpy(matrixA12+(i*(dimension/2)),matrixBufferA+(dimension*i)+(dimension/2),  sizeof(char)*dimension/2);
                memcpy(matrixA21+(i*(dimension/2)),matrixBufferA+(dimension*i)+(dimension/2)*dimension,  sizeof(char)*dimension/2);
                memcpy(matrixA22+(i*(dimension/2)),matrixBufferA+(dimension*i)+(dimension/2)*dimension+(dimension/2),  sizeof(char)*dimension/2);
        }

        for(int i = 0; i < dimension/2; i++)
        {
                memcpy(matrixB11+(i*(dimension/2)),matrixBufferB+(dimension*i), sizeof(char)*dimension/2);
                memcpy(matrixB12+(i*(dimension/2)),matrixBufferB+(dimension*i)+(dimension/2),  sizeof(char)*dimension/2);
                memcpy(matrixB21+(i*(dimension/2)),matrixBufferB+(dimension*i)+(dimension/2)*dimension,  sizeof(char)*dimension/2);
                memcpy(matrixB22+(i*(dimension/2)),matrixBufferB+(dimension*i)+(dimension/2)*dimension+(dimension/2),  sizeof(char)*dimension/2);
        }

        /***************************************************************************************/
        /***********************************PROCESS 2*******************************************/
        /***************************************************************************************/
        switch (p2 = fork())
        {
        case -1:
                fprintf(stderr, "fork p2 error: %s", strerror(errno));
                exit(EXIT_FAILURE);
                break;
        case 0: 
        {
                dimension = dimension/2;
                sa.sa_handler = sendChild; //Set sendChild  as SIGINT handler 
                if(sigaction(SIGINT, &sa, NULL) == -1)
                {
                        fprintf(stderr, "sigaction SIGINT handler: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                sa.sa_handler = termChild; //Set termChild  as SIGTERM handler
                if(sigaction(SIGTERM, &sa, NULL) == -1)
                {
                        fprintf(stderr, "sigaction SIGTERM handler: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                if(sigprocmask(SIG_UNBLOCK, &intBlock, NULL) == -1) //Signal handlers are setted. Now we can unblock them.
                {
                        fprintf(stderr, "unblock SIGINT p2 handler: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }

                int* matrixC11 = (int*) malloc(sizeof(int)*dimension*dimension);//Create quarters for result and take as input
                if(!matrixC11)
                {
                        fprintf(stderr, "malloc matrixC11 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(matrixC11);

                int* quarterA1 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterA1)
                {
                        fprintf(stderr, "malloc quarterA1 p2 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterA1);

                int* quarterB1 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterB1)
                {
                        fprintf(stderr, "malloc quarterB1 p2 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterB1);

                int* quarterA2 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterA2)
                {
                        fprintf(stderr, "malloc quarterA2 p2 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterA2);

                int* quarterB2 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterB2)
                {
                        fprintf(stderr, "malloc quarterB2 p2 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterB2);

                int* quarterC1 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterC1)
                {
                        fprintf(stderr, "malloc quarterC1 p2 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterC1);

                int* quarterC2 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterC2)
                {
                        fprintf(stderr, "malloc quarterC2 p2 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterC2);
        
                if(close(p2Pipe[1]) < 0)//writing end of p2 closed
                {
                        fprintf(stderr, "pipe p2 writing end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                if(close(p2PipeRev[0]) < 0)//reading end of p2Rev closed
                {
                        fprintf(stderr, "pipe p2Rev reading end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                if(close(barrier[0]) < 0)//reading end of barrier closed
                {
                        fprintf(stderr, "barrier p2 reading end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                
                /****************************TAKE A11, B11 and A12, B21 as INPUT THROGH PIPE************/ 
                for(int i = 0; i < dimension; i++)
                {
                        for(int j = 0; j < dimension; j++)
                        {
                                status = read(p2Pipe[0], &quarterA1[(i*dimension)+j], sizeof(quarterA1[(i*dimension)+j]));
                                if(status == -1)
                                {
                                        fprintf(stderr, "read p2 quarterA1 error: %s", strerror(errno));
                                        exit(EXIT_FAILURE);
                                }
                        }
                }
                for(int i = 0; i < dimension; i++)
                {
                        for(int j = 0; j < dimension; j++)
                        {
                                status = read(p2Pipe[0], &quarterB1[(i*dimension)+j], sizeof(quarterB1[(i*dimension)+j]));
                                if(status == -1)
                                {
                                        fprintf(stderr, "read p2 quarterB1 error: %s", strerror(errno));
                                        exit(EXIT_FAILURE);
                                }
                        }
                }
                for(int i = 0; i < dimension; i++)
                {
                        for(int j = 0; j < dimension; j++)
                        {
                                status = read(p2Pipe[0], &quarterA2[(i*dimension)+j], sizeof(quarterA2[(i*dimension)+j]));
                                if(status == -1)
                                {
                                        fprintf(stderr, "read p2 quarterA2 error: %s", strerror(errno));
                                        exit(EXIT_FAILURE);
                                }
                        }
                }
                for(int i = 0; i < dimension; i++)
                {
                        for(int j = 0; j < dimension; j++)
                        {
                                status = read(p2Pipe[0], &quarterB2[(i*dimension)+j], sizeof(quarterB2[(i*dimension)+j]));
                                if(status == -1)
                                {
                                        fprintf(stderr, "read p2 quarterB2 error: %s", strerror(errno));
                                        exit(EXIT_FAILURE);
                                }
                        }
                }
                
                /*************DO CALCULATION AND CREATE RESULT QUARTER*********/
                multiply(quarterA1, quarterB1, quarterC1, dimension);
                multiply(quarterA2, quarterB2, quarterC2, dimension);
                sum(quarterC1, quarterC2, matrixC11, dimension);

                if(close(barrier[1]) < 0)//writing end of barrier closed. Process 2 reached barrier
                {
                        fprintf(stderr, "barrier p2 writing end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }        

                //Send result quarter to parent process through revPipe
                for(int i = 0; i < dimension; i++)
                        for(int j = 0; j < dimension; j++)
                                write(p2PipeRev[1], &matrixC11[(i*dimension)+j], sizeof(matrixC11[(i*dimension)+j]));
                
                if(close(p2Pipe[0]) < 0)//reading end of p2 closed
                {
                        fprintf(stderr, "pipe p2 reading end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                if(close(p2PipeRev[1]) < 0)//writing end of p2 closed
                {
                        fprintf(stderr, "pipe p2Rev writing end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }    

                exit(EXIT_SUCCESS);
                break;
        }
        default:
                break;
        }

        if(close(p2Pipe[0]) < 0)//reading end of p2 closed
        {
                fprintf(stderr, "pipe p2 reading end close error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        if(close(p2PipeRev[1]) < 0)//writing end of p2 closed
        {
                fprintf(stderr, "pipe p2Rev writing end close error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }    

        /****************************SEND A11, B11 and A12, B21 as INPUT TO P2 THROGH PIPE************/ 
        for(int i = 0; i < dimension/2; i++)
        {
                for(int j = 0; j < dimension/2; j++)
                {
                        numTemp = (int) matrixA11[(i*(dimension/2)+j)];
                        status = write(p2Pipe[1], &numTemp, sizeof(numTemp));
                        if(status == -1)
                        {
                                fprintf(stderr, "write p2 matrixA11 error: %s", strerror(errno));
                                exit(EXIT_FAILURE);
                        }
                }
        }
        for(int i = 0; i < dimension/2; i++)
        {
                for(int j = 0; j < dimension/2; j++)
                {
                        numTemp = (int) matrixB11[(i*(dimension/2)+j)];
                        status = write(p2Pipe[1], &numTemp, sizeof(numTemp));
                        if(status == -1)
                        {
                                fprintf(stderr, "write p2 matrixB11 error: %s", strerror(errno));
                                exit(EXIT_FAILURE);
                        }
                }
        }

        for(int i = 0; i < dimension/2; i++)
        {
                for(int j = 0; j < dimension/2; j++)
                {
                        numTemp = (int) matrixA12[(i*(dimension/2)+j)];
                        status = write(p2Pipe[1], &numTemp, sizeof(numTemp));
                        if(status == -1)
                        {
                                fprintf(stderr, "write p2 matrixA12 error: %s", strerror(errno));
                                exit(EXIT_FAILURE);
                        }
                }
        }

        for(int i = 0; i < dimension/2; i++)
        {
                for(int j = 0; j < dimension/2; j++)
                {
                        numTemp = (int) matrixB21[(i*(dimension/2)+j)];
                        status = write(p2Pipe[1], &numTemp, sizeof(numTemp));
                        if(status == -1)
                        {
                                fprintf(stderr, "write p2 matrixB21 error: %s", strerror(errno));
                                exit(EXIT_FAILURE);
                        }
                }
        }

        /***************************************************************************************/
        /***********************************PROCESS 3*******************************************/
        /***************************************************************************************/
        switch (p3 = fork())
        {
        case -1:
                fprintf(stderr, "fork p3 error: %s", strerror(errno));
                exit(EXIT_FAILURE);
                break;
        case 0: 
        {
                dimension = dimension/2;
                sa.sa_handler = sendChild;//Set sendChild  as SIGINT handler
                if(sigaction(SIGINT, &sa, NULL) == -1)
                {
                        fprintf(stderr, "sigaction SIGINT handler: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                sa.sa_handler = termChild;//Set termChild  as SIGTERM handler
                if(sigaction(SIGTERM, &sa, NULL) == -1)
                {
                        fprintf(stderr, "sigaction SIGTERM handler: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                if(sigprocmask(SIG_UNBLOCK, &intBlock, NULL) == -1)//Signal handlers are setted. Now we can unblock them.
                {
                        fprintf(stderr, "unblock SIGINT p3 handler: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }

                int* matrixC12 = (int*) malloc(sizeof(int)*dimension*dimension);//Create quarters for result and take as input
                if(!matrixC12)
                {
                        fprintf(stderr, "malloc matrixC12 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(matrixC12);

                int* quarterA1 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterA1)
                {
                        fprintf(stderr, "malloc quarterA1 p3 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterA1);

                int* quarterB1 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterB1)
                {
                        fprintf(stderr, "malloc quarterB1 p3 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterB1);

                int* quarterA2 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterA2)
                {
                        fprintf(stderr, "malloc quarterA2 p3 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterA2);

                int* quarterB2 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterB2)
                {
                        fprintf(stderr, "malloc quarterB2 p3 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterB2);

                int* quarterC1 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterC1)
                {
                        fprintf(stderr, "malloc quarterC1 p3 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterC1);

                int* quarterC2 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterC2)
                {
                        fprintf(stderr, "malloc quarterC2 p3 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterC2);
        
                if(close(p3Pipe[1]) < 0)//writing end of p3 closed
                {
                        fprintf(stderr, "pipe p3 writing end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                if(close(p3PipeRev[0]) < 0)//reading end of p3Rev closed
                {
                        fprintf(stderr, "pipe p3Rev reading end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                if(close(barrier[0]) < 0)//reading end of barrier closed
                {
                        fprintf(stderr, "barrier p3 reading end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                /****************************TAKE A11, B12 and A12, B22 as INPUT THROGH PIPE************/ 
                for(int i = 0; i < dimension; i++)
                {
                        for(int j = 0; j < dimension; j++)
                        {
                                status = read(p3Pipe[0], &quarterA1[(i*dimension)+j], sizeof(quarterA1[(i*dimension)+j]));
                                if(status == -1)
                                {
                                        fprintf(stderr, "read p3 quarterA1 error: %s", strerror(errno));
                                        exit(EXIT_FAILURE);
                                }
                        }
                }
                for(int i = 0; i < dimension; i++)
                {
                        for(int j = 0; j < dimension; j++)
                        {
                                status = read(p3Pipe[0], &quarterB1[(i*dimension)+j], sizeof(quarterB1[(i*dimension)+j]));
                                if(status == -1)
                                {
                                        fprintf(stderr, "read p3 quarterB1 error: %s", strerror(errno));
                                        exit(EXIT_FAILURE);
                                }
                        }
                }
                for(int i = 0; i < dimension; i++)
                {
                        for(int j = 0; j < dimension; j++)
                        {
                                status = read(p3Pipe[0], &quarterA2[(i*dimension)+j], sizeof(quarterA2[(i*dimension)+j]));
                                if(status == -1)
                                {
                                        fprintf(stderr, "read p3 quarterA2 error: %s", strerror(errno));
                                        exit(EXIT_FAILURE);
                                }
                        }
                }
                for(int i = 0; i < dimension; i++)
                {
                        for(int j = 0; j < dimension; j++)
                        {
                                status = read(p3Pipe[0], &quarterB2[(i*dimension)+j], sizeof(quarterB2[(i*dimension)+j]));
                                if(status == -1)
                                {
                                        fprintf(stderr, "read p3 quarterB2 error: %s", strerror(errno));
                                        exit(EXIT_FAILURE);
                                }
                        }
                }
                /*************DO CALCULATION AND CREATE RESULT QUARTER*********/
                multiply(quarterA1, quarterB1, quarterC1, dimension);
                multiply(quarterA2, quarterB2, quarterC2, dimension);
                sum(quarterC1, quarterC2, matrixC12, dimension);

                if(close(barrier[1]) < 0)//writing end of barrier closed. Process 3 reached barrier
                {
                        fprintf(stderr, "barrier p3 writing end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }    

                //Send result quarter to parent process through revPipe
                for(int i = 0; i < dimension; i++)
                        for(int j = 0; j < dimension; j++)
                                write(p3PipeRev[1], &matrixC12[(i*dimension)+j], sizeof(matrixC12[(i*dimension)+j]));
                
                if(close(p3Pipe[0]) < 0)//reading end of p3 closed
                {
                        fprintf(stderr, "pipe p3 reading end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                if(close(p3PipeRev[1]) < 0)//writing end of p3 closed
                {
                        fprintf(stderr, "pipe p3Rev writing end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }    

                exit(EXIT_SUCCESS);
                break;
        }
        default:
                break;
        }

        if(close(p3Pipe[0]) < 0)//reading end of p3 closed
        {
                fprintf(stderr, "pipe p3 reading end close error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        if(close(p3PipeRev[1]) < 0)//writing end of p3 closed
        {
                fprintf(stderr, "pipe p3Rev writing end close error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }    
        /****************************SEND A11, B12 and A12, B22 as INPUT TO P3 THROGH PIPE************/
        for(int i = 0; i < dimension/2; i++)
        {
                for(int j = 0; j < dimension/2; j++)
                {
                        numTemp = (int) matrixA11[(i*(dimension/2)+j)];
                        status = write(p3Pipe[1], &numTemp, sizeof(numTemp));
                        if(status == -1)
                        {
                                fprintf(stderr, "write p3 matrixA11 error: %s", strerror(errno));
                                exit(EXIT_FAILURE);
                        }
                }
        }
        for(int i = 0; i < dimension/2; i++)
        {
                for(int j = 0; j < dimension/2; j++)
                {
                        numTemp = (int) matrixB12[(i*(dimension/2)+j)];
                        status = write(p3Pipe[1], &numTemp, sizeof(numTemp));
                        if(status == -1)
                        {
                                fprintf(stderr, "write p3 matrixB12 error: %s", strerror(errno));
                                exit(EXIT_FAILURE);
                        }
                }
        }

        for(int i = 0; i < dimension/2; i++)
        {
                for(int j = 0; j < dimension/2; j++)
                {
                        numTemp = (int) matrixA12[(i*(dimension/2)+j)];
                        status = write(p3Pipe[1], &numTemp, sizeof(numTemp));
                        if(status == -1)
                        {
                                fprintf(stderr, "write p3 matrixA12 error: %s", strerror(errno));
                                exit(EXIT_FAILURE);
                        }
                }
        }

        for(int i = 0; i < dimension/2; i++)
        {
                for(int j = 0; j < dimension/2; j++)
                {
                        numTemp = (int) matrixB22[(i*(dimension/2)+j)];
                        status = write(p3Pipe[1], &numTemp, sizeof(numTemp));
                        if(status == -1)
                        {
                                fprintf(stderr, "write p3 matrixB22 error: %s", strerror(errno));
                                exit(EXIT_FAILURE);
                        }
                }
        }

        /***************************************************************************************/
        /***********************************PROCESS 4*******************************************/
        /***************************************************************************************/
        switch (p4 = fork())
        {
        case -1:
                fprintf(stderr, "fork p4 error: %s", strerror(errno));
                exit(EXIT_FAILURE);
                break;
        case 0: 
        {
                dimension = dimension/2;
                sa.sa_handler = sendChild;//Set sendChild  as SIGINT handler
                if(sigaction(SIGINT, &sa, NULL) == -1)
                {
                        fprintf(stderr, "sigaction SIGINT handler: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                sa.sa_handler = termChild;//Set termChild  as SIGTERM handler
                if(sigaction(SIGTERM, &sa, NULL) == -1)
                {
                        fprintf(stderr, "sigaction SIGTERM handler: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                if(sigprocmask(SIG_UNBLOCK, &intBlock, NULL) == -1)//Signal handlers are setted. Now we can unblock them.
                {
                        fprintf(stderr, "unblock SIGINT p4 handler: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }

                int* matrixC21 = (int*) malloc(sizeof(int)*dimension*dimension);//Create quarters for result and take as input
                if(!matrixC21)
                {
                        fprintf(stderr, "malloc matrixC21 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(matrixC21);

                int* quarterA1 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterA1)
                {
                        fprintf(stderr, "malloc quarterA1 p4 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterA1);

                int* quarterB1 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterB1)
                {
                        fprintf(stderr, "malloc quarterB1 p4 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterB1);

                int* quarterA2 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterA2)
                {
                        fprintf(stderr, "malloc quarterA2 p4 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterA2);

                int* quarterB2 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterB2)
                {
                        fprintf(stderr, "malloc quarterB2 p4 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterB2);

                int* quarterC1 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterC1)
                {
                        fprintf(stderr, "malloc quarterC1 p4 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterC1);

                int* quarterC2 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterC2)
                {
                        fprintf(stderr, "malloc quarterC2 p4 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterC2);
        
                if(close(p4Pipe[1]) < 0)//writing end of p4 closed
                {
                        fprintf(stderr, "pipe p4 writing end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                if(close(p4PipeRev[0]) < 0)//reading end of p4Rev closed
                {
                        fprintf(stderr, "pipe p4Rev reading end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }

                if(close(barrier[0]) < 0)//reading end of barrier closed
                {
                        fprintf(stderr, "barrier p4 reading end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }

                /****************************TAKE A21, B11 and A22, B21 as INPUT THROGH PIPE************/ 
                for(int i = 0; i < dimension; i++)
                {
                        for(int j = 0; j < dimension; j++)
                        {
                                status = read(p4Pipe[0], &quarterA1[(i*dimension)+j], sizeof(quarterA1[(i*dimension)+j]));
                                if(status == -1)
                                {
                                        fprintf(stderr, "read p4 quarterA1 error: %s", strerror(errno));
                                        exit(EXIT_FAILURE);
                                }
                        }
                }
                for(int i = 0; i < dimension; i++)
                {
                        for(int j = 0; j < dimension; j++)
                        {
                                status = read(p4Pipe[0], &quarterB1[(i*dimension)+j], sizeof(quarterB1[(i*dimension)+j]));
                                if(status == -1)
                                {
                                        fprintf(stderr, "read p4 quarterB1 error: %s", strerror(errno));
                                        exit(EXIT_FAILURE);
                                }
                        }
                }
                for(int i = 0; i < dimension; i++)
                {
                        for(int j = 0; j < dimension; j++)
                        {
                                status = read(p4Pipe[0], &quarterA2[(i*dimension)+j], sizeof(quarterA2[(i*dimension)+j]));
                                if(status == -1)
                                {
                                        fprintf(stderr, "read p4 quarterA2 error: %s", strerror(errno));
                                        exit(EXIT_FAILURE);
                                }
                        }
                }
                for(int i = 0; i < dimension; i++)
                {
                        for(int j = 0; j < dimension; j++)
                        {
                                status = read(p4Pipe[0], &quarterB2[(i*dimension)+j], sizeof(quarterB2[(i*dimension)+j]));
                                if(status == -1)
                                {
                                        fprintf(stderr, "read p4 quarterB2 error: %s", strerror(errno));
                                        exit(EXIT_FAILURE);
                                }
                        }
                }
                /*************DO CALCULATION AND CREATE RESULT QUARTER*********/
                multiply(quarterA1, quarterB1, quarterC1, dimension);
                multiply(quarterA2, quarterB2, quarterC2, dimension);
                sum(quarterC1, quarterC2, matrixC21, dimension);

                if(close(barrier[1]) < 0)//writing end of barrier closed. Process 4 reached barrier
                {
                        fprintf(stderr, "barrier p4 writing end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }    

                //Send result quarter to parent process through revPipe
                for(int i = 0; i < dimension; i++)
                        for(int j = 0; j < dimension; j++)
                                write(p4PipeRev[1], &matrixC21[(i*dimension)+j], sizeof(matrixC21[(i*dimension)+j]));
                
                if(close(p4Pipe[0]) < 0)//reading end of p4 closed
                {
                        fprintf(stderr, "pipe p4 reading end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                if(close(p4PipeRev[1]) < 0)//writing end of p4 closed
                {
                        fprintf(stderr, "pipe p4Rev writing end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }    

                exit(EXIT_SUCCESS);
                break;
        }
        default:
                break;
        }

        if(close(p4Pipe[0]) < 0)//reading end of p4 closed
        {
                fprintf(stderr, "pipe p4 reading end close error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        if(close(p4PipeRev[1]) < 0)//writing end of p4 closed
        {
                fprintf(stderr, "pipe p4Rev writing end close error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }    
        /****************************SEND A21, B11 and A22, B21 as INPUT TO P4 THROGH PIPE************/ 
        for(int i = 0; i < dimension/2; i++)
        {
                for(int j = 0; j < dimension/2; j++)
                {
                        numTemp = (int) matrixA21[(i*(dimension/2)+j)];
                        status = write(p4Pipe[1], &numTemp, sizeof(numTemp));
                        if(status == -1)
                        {
                                fprintf(stderr, "write p4 matrixA21 error: %s", strerror(errno));
                                exit(EXIT_FAILURE);
                        }
                }
        }
        for(int i = 0; i < dimension/2; i++)
        {
                for(int j = 0; j < dimension/2; j++)
                {
                        numTemp = (int) matrixB11[(i*(dimension/2)+j)];
                        status = write(p4Pipe[1], &numTemp, sizeof(numTemp));
                        if(status == -1)
                        {
                                fprintf(stderr, "write p4 matrixB11 error: %s", strerror(errno));
                                exit(EXIT_FAILURE);
                        }
                }
        }

        for(int i = 0; i < dimension/2; i++)
        {
                for(int j = 0; j < dimension/2; j++)
                {
                        numTemp = (int) matrixA22[(i*(dimension/2)+j)];
                        status = write(p4Pipe[1], &numTemp, sizeof(numTemp));
                        if(status == -1)
                        {
                                fprintf(stderr, "write p4 matrixA22 error: %s", strerror(errno));
                                exit(EXIT_FAILURE);
                        }
                }
        }

        for(int i = 0; i < dimension/2; i++)
        {
                for(int j = 0; j < dimension/2; j++)
                {
                        numTemp = (int) matrixB21[(i*(dimension/2)+j)];
                        status = write(p4Pipe[1], &numTemp, sizeof(numTemp));
                        if(status == -1)
                        {
                                fprintf(stderr, "write p4 matrixB21 error: %s", strerror(errno));
                                exit(EXIT_FAILURE);
                        }
                }
        }

        /***************************************************************************************/
        /***********************************PROCESS 5*******************************************/
        /***************************************************************************************/
        switch (p5 = fork())
        {
        case -1:
                fprintf(stderr, "fork p5 error: %s", strerror(errno));
                exit(EXIT_FAILURE);
                break;
        case 0: 
        {
                dimension = dimension/2;
                sa.sa_handler = sendChild;//Set sendChild  as SIGINT handler
                if(sigaction(SIGINT, &sa, NULL) == -1)
                {
                        fprintf(stderr, "sigaction SIGINT handler: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                sa.sa_handler = termChild;//Set termChild  as SIGTERM handler
                if(sigaction(SIGTERM, &sa, NULL) == -1)
                {
                        fprintf(stderr, "sigaction SIGTERM handler: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                if(sigprocmask(SIG_UNBLOCK, &intBlock, NULL) == -1)//Signal handlers are setted. Now we can unblock them.
                {
                        fprintf(stderr, "unblock SIGINT p5 handler: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }

                int* matrixC22 = (int*) malloc(sizeof(int)*dimension*dimension);//Create quarters for result and take as input
                if(!matrixC22)
                {
                        fprintf(stderr, "malloc matrixC22 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(matrixC22);

                int* quarterA1 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterA1)
                {
                        fprintf(stderr, "malloc quarterA1 p5 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterA1);

                int* quarterB1 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterB1)
                {
                        fprintf(stderr, "malloc quarterB1 p5 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterB1);

                int* quarterA2 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterA2)
                {
                        fprintf(stderr, "malloc quarterA2 p5 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterA2);

                int* quarterB2 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterB2)
                {
                        fprintf(stderr, "malloc quarterB2 p5 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterB2);

                int* quarterC1 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterC1)
                {
                        fprintf(stderr, "malloc quarterC1 p5 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterC1);

                int* quarterC2 = (int*) malloc(sizeof(int)*dimension*dimension);
                if(!quarterC2)
                {
                        fprintf(stderr, "malloc quarterC2 p5 error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                addPointer(quarterC2);
        
                if(close(p5Pipe[1]) < 0)//writing end of p5 closed
                {
                        fprintf(stderr, "pipe p5 writing end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                if(close(p5PipeRev[0]) < 0)//reading end of p5Rev closed
                {
                        fprintf(stderr, "pipe p5Rev reading end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                if(close(barrier[0]) < 0)//reading end of barrier closed
                {
                        fprintf(stderr, "barrier p5 reading end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }

                /****************************TAKE A21, B12 and A22, B22 as INPUT THROGH PIPE************/ 
                for(int i = 0; i < dimension; i++)
                {
                        for(int j = 0; j < dimension; j++)
                        {
                                status = read(p5Pipe[0], &quarterA1[(i*dimension)+j], sizeof(quarterA1[(i*dimension)+j]));
                                if(status == -1)
                                {
                                        fprintf(stderr, "read p5 quarterA1 error: %s", strerror(errno));
                                        exit(EXIT_FAILURE);
                                }
                        }
                }
                for(int i = 0; i < dimension; i++)
                {
                        for(int j = 0; j < dimension; j++)
                        {
                                status = read(p5Pipe[0], &quarterB1[(i*dimension)+j], sizeof(quarterB1[(i*dimension)+j]));
                                if(status == -1)
                                {
                                        fprintf(stderr, "read p5 quarterB1 error: %s", strerror(errno));
                                        exit(EXIT_FAILURE);
                                }
                        }
                }
                for(int i = 0; i < dimension; i++)
                {
                        for(int j = 0; j < dimension; j++)
                        {
                                status = read(p5Pipe[0], &quarterA2[(i*dimension)+j], sizeof(quarterA2[(i*dimension)+j]));
                                if(status == -1)
                                {
                                        fprintf(stderr, "read p5 quarterA2 error: %s", strerror(errno));
                                        exit(EXIT_FAILURE);
                                }
                        }
                }
                for(int i = 0; i < dimension; i++)
                {
                        for(int j = 0; j < dimension; j++)
                        {
                                status = read(p5Pipe[0], &quarterB2[(i*dimension)+j], sizeof(quarterB2[(i*dimension)+j]));
                                if(status == -1)
                                {
                                        fprintf(stderr, "read p5 quarterB2 error: %s", strerror(errno));
                                        exit(EXIT_FAILURE);
                                }
                        }
                }
                 /*************DO CALCULATION AND CREATE RESULT QUARTER*********/
                multiply(quarterA1, quarterB1, quarterC1, dimension);
                multiply(quarterA2, quarterB2, quarterC2, dimension);
                sum(quarterC1, quarterC2, matrixC22, dimension);

                if(close(barrier[1]) < 0)//writing end of barrier closed. Process 5 reached barrier
                {
                        fprintf(stderr, "barrier p5 writing end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }    

                //Send result quarter to parent process through revPipe
                for(int i = 0; i < dimension; i++)
                        for(int j = 0; j < dimension; j++)
                                write(p5PipeRev[1], &matrixC22[(i*dimension)+j], sizeof(matrixC22[(i*dimension)+j]));
                
                if(close(p5Pipe[0]) < 0)//reading end of p5 closed
                {
                        fprintf(stderr, "pipe p5 reading end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
                if(close(p5PipeRev[1]) < 0)//writing end of p5 closed
                {
                        fprintf(stderr, "pipe p5Rev writing end close error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }    

                exit(EXIT_SUCCESS);
                break;
        }
        default:
                break;
        }

        if(close(p5Pipe[0]) < 0)//reading end of p5 closed
        {
                fprintf(stderr, "pipe p5 reading end close error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        if(close(p5PipeRev[1]) < 0)//writing end of p5 closed
        {
                fprintf(stderr, "pipe p5Rev writing end close error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }    
        /****************************SEND A21, B12 and A22, B22 as INPUT TO P5 THROGH PIPE************/
        for(int i = 0; i < dimension/2; i++)
        {
                for(int j = 0; j < dimension/2; j++)
                {
                        numTemp = (int) matrixA21[(i*(dimension/2)+j)];
                        status = write(p5Pipe[1], &numTemp, sizeof(numTemp));
                        if(status == -1)
                        {
                                fprintf(stderr, "write p5 matrixA21 error: %s", strerror(errno));
                                exit(EXIT_FAILURE);
                        }
                }
        }
        for(int i = 0; i < dimension/2; i++)
        {
                for(int j = 0; j < dimension/2; j++)
                {
                        numTemp = (int) matrixB12[(i*(dimension/2)+j)];
                        status = write(p5Pipe[1], &numTemp, sizeof(numTemp));
                        if(status == -1)
                        {
                                fprintf(stderr, "write p5 matrixB12 error: %s", strerror(errno));
                                exit(EXIT_FAILURE);
                        }
                }
        }

        for(int i = 0; i < dimension/2; i++)
        {
                for(int j = 0; j < dimension/2; j++)
                {
                        numTemp = (int) matrixA22[(i*(dimension/2)+j)];
                        status = write(p5Pipe[1], &numTemp, sizeof(numTemp));
                        if(status == -1)
                        {
                                fprintf(stderr, "write p5 matrixA22 error: %s", strerror(errno));
                                exit(EXIT_FAILURE);
                        }
                }
        }

        for(int i = 0; i < dimension/2; i++)
        {
                for(int j = 0; j < dimension/2; j++)
                {
                        numTemp = (int) matrixB22[(i*(dimension/2)+j)];
                        status = write(p5Pipe[1], &numTemp, sizeof(numTemp));
                        if(status == -1)
                        {
                                fprintf(stderr, "write p5 matrixB22 error: %s", strerror(errno));
                                exit(EXIT_FAILURE);
                        }
                }
        }

        if(close(barrier[1]) < 0)//writing end of barrier closed
        {
                fprintf(stderr, "barrier writing end close error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }

        int num;
        fprintf(stdout, "Calculations started...\n\n");
        /*******************************BARRIER*****************************/
        if(read(barrier[0], &num, sizeof(num)) != 0) //This is the barrier. Until all childs reach there, parent will be blocked
        {
                fprintf(stderr, "barrier continue read error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        fprintf(stdout, "Calculations ended. All processes reached barrier...\n\n");
        /********************TAKE OUTPUTS OF CHILDS AS SINGULAR VALUES AND RESEMBLE THEM. PRINT TO SCREEN*****************/
         fprintf(stdout, "\n**************MATRIX C**************\n");
        for(int i = 0; i < dimension; i++)
        {
                for(int j = 0; j < dimension; j++)
                {
                        if(j%dimension == 0)
                                fprintf(stdout, "\n");
                        if(i < (dimension/2) && j < (dimension/2))
                        {
                                if (read(p2PipeRev[0], &num, sizeof(num)) < 0)
                                {
                                        fprintf(stderr, "read p1 p2PipeRev error: %s", strerror(errno));
                                        exit(EXIT_FAILURE);
                                }
                                fprintf(stdout, " %6d ", num);
                        }
                        else if(i < (dimension/2) && j >= (dimension/2))
                        {
                                if (read(p3PipeRev[0], &num, sizeof(num)) < 0)
                                {
                                        fprintf(stderr, "read p1 p3PipeRev error: %s", strerror(errno));
                                        exit(EXIT_FAILURE);
                                }
                                fprintf(stdout, " %6d ", num);
                        }
                        else if(i >= (dimension/2) && j < (dimension/2))
                        {
                                if (read(p4PipeRev[0], &num, sizeof(num)) < 0)
                                {
                                        fprintf(stderr, "read p1 p4PipeRev error: %s", strerror(errno));
                                        exit(EXIT_FAILURE);
                                }
                                fprintf(stdout, " %6d ", num);
                        }
                        else{
                                if (read(p5PipeRev[0], &num, sizeof(num)) < 0)
                                {
                                        fprintf(stderr, "read p1 p5PipeRev error: %s", strerror(errno));
                                        exit(EXIT_FAILURE);
                                }
                                fprintf(stdout, " %6d ", num);
                        }
                }
        }

        fprintf(stdout, "\n");

        if(close(p2Pipe[1]) < 0)//writing end of p2 closed
        {
                fprintf(stderr, "pipe p2 reading end close error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        if(close(p2PipeRev[0]) < 0)//reading end of p2Rev closed
        {
                fprintf(stderr, "pipe p2Rev writing end close error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }

        if(close(p3Pipe[1]) < 0)//writing end of p3 closed
        {
                fprintf(stderr, "pipe p3 reading end close error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        if(close(p3PipeRev[0]) < 0)//reading end of p3Rev closed
        {
                fprintf(stderr, "pipe p3Rev writing end close error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }

        if(close(p4Pipe[1]) < 0)//writing end of p4 closed
        {
                fprintf(stderr, "pipe p4 reading end close error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        if(close(p4PipeRev[0]) < 0)//reading end of p4Rev closed
        {
                fprintf(stderr, "pipe p4Rev writing end close error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }

        if(close(p5Pipe[1]) < 0)//writing end of p5 closed
        {
                fprintf(stderr, "pipe p5 writing end close error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }
        if(close(p5PipeRev[0]) < 0)//reading end of p5Rev closed
        {
                fprintf(stderr, "pipe p5Rev reading end close error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }

        if(close(barrier[0]) < 0)//reading end of barrier closed
        {
                fprintf(stderr, "barrier reading end close error: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }

        sigemptyset(&emptyMask);
        while(liveChild > 0)
        {
                //Suspend parent process until receive SIGCHILD delivered.
                //From there by SIGCHLD handler reap childs and take their status values.
                if(sigsuspend(&emptyMask) == -1 && errno != EINTR) 
                {
                        fprintf(stderr, "sisgsuspen emptyMask error: %s", strerror(errno));
                        exit(EXIT_FAILURE);
                }
        }

        fprintf(stdout, "\n\nProcess done!!!\n");

        exit(EXIT_SUCCESS);
}
