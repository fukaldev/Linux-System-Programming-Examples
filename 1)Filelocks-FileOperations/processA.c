/*
 * =====================================================================================
 *
 *       Filename:  processA.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08-03-2020 15:27:44
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Furkan Kalabalık, 
 *   Organization:  
 *
 * =====================================================================================
 */


#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

void convertComplex(char* twoByte, char** complexBuffer);

int main(int argc, char* argv[]){
	int inputFd, outputFd, openFlags;
	mode_t filePerms;
	struct flock lock, writeLock, readLock;
	lock.l_type    = F_WRLCK;   /* Test for any lock on any part of file. */
 	lock.l_start   = 0;
 	lock.l_whence  = SEEK_SET;
 	lock.l_len     = 0;
	lock.l_pid     = getpid();        
 	writeLock = lock;//WRITE LOCK STRUCT
	lock.l_type = F_RDLCK;
	readLock = lock;//READ LOCK STRUCT
	char lineBuffer[160];//
	char tempBuffer[160];
	char buf[32];
	char checkBuf[1];
	char* complexBuffer;
	char* inputName;
	char* outputName;
	char* sleepTimeChar;
	int sleepTime;
	int c;
	while ((c = getopt (argc, argv, ":i:o:t:")) != -1)
		switch (c)
      			{
      			case 'i':
				inputName = optarg;
        			break;
      			case 'o':
				outputName = optarg;
        			break;
      			case 't':
				sleepTimeChar = optarg;	
				sleepTime = atoi(sleepTimeChar);
        			break;
      			default:
        			printf("Format must be -i [inputFile] -o [outputFile] -t [sleepTime]\n");
				return EXIT_SUCCESS;
      			}
	if(inputName == NULL | outputName == NULL | sleepTimeChar == NULL){
		printf("Format must be -i [inputFile] -o [outputFile] -t [sleepTime]\n");
                return EXIT_SUCCESS;
	}
	inputFd = open(inputName, O_RDONLY | O_SYNC);
	if(inputFd == -1){
		perror("OPENING INPUT FAILURE: ");
		exit(EXIT_FAILURE);
	}
	
	outputFd = open(outputName, O_RDWR | O_SYNC);
	if(outputFd == -1){
		close(inputFd);
                perror("OPENING OUTPUT FAILURE: ");
                exit(EXIT_FAILURE);
        }
	if(fcntl(outputFd, F_GETLK, &lock) == -1){//GET ANY FILE LOCK ON FILE
		close(inputFd);
		close(outputFd);
		perror("FILE LOCK GET FAIL: ");
		exit(EXIT_FAILURE);	
	} 
 	while(read(inputFd, buf, 32) == 32){
		for(int i = 0; i < 32; i = i+2){
			convertComplex(&buf[i], &complexBuffer);//CONVET TWO BYTE STRING CHUNK TO COMPLEX NUMBER
			if(i == 0)
				strcpy(lineBuffer, complexBuffer);
			else
				strcat(lineBuffer, complexBuffer);
			if(i != 30)
				strcat(lineBuffer, ",");
			else
				strcat(lineBuffer, "\n");
		}
		
		while((lock.l_type == F_RDLCK || lock.l_type == F_WRLCK)  && lock.l_pid != getpid()){
                        printf("My pid: %d, lock pid: %d\n", getpid(), lock.l_pid);
                        printf("Other process doing\n");
                        if(fcntl(outputFd, F_GETLK, &lock) == -1){
                                close(inputFd);
                                close(outputFd);
                                perror("FILE LOCK GET FAIL: ");
                		exit(EXIT_FAILURE); 
                        }
                }//LOOK FOR ANY LOCK ON FILE
		if(fcntl(outputFd, F_SETLKW, &readLock) == -1){
                        close(inputFd);
                        close(outputFd);
			perror("FILE LOCK SET FAIL: ");
	                exit(EXIT_FAILURE);
                }//LOCK ON OUTPUT FILE
		lseek(outputFd, 0, SEEK_SET);
		int readStatus;
		int numberOfChar = 0;
		int emptyLine = 0;
		int numberOfBytesToTemp = 0;
		while(!emptyLine){
			readStatus = read(outputFd, checkBuf, 1);
			if(readStatus == -1){
				close(inputFd);
                        	close(outputFd);
                        	perror("OUTPUT FILE READ FAIL: ");
                		exit(EXIT_FAILURE);
			} 
			if(readStatus == 0)
				break;
			if(checkBuf[0] == '\n' && numberOfChar == 0){
				emptyLine = 1;
				lseek(outputFd, -1, SEEK_CUR);

			}
			else if((checkBuf[0] == '\n' && numberOfChar != 0)){
				numberOfChar = 0;
			}
			else
				numberOfChar++; 
				
		}//FIND AN EMPTY LINE TO WRITE
		int tempFd = open("temp.txt", O_CREAT | O_RDWR | O_TRUNC);
		if(tempFd == -1){
                	close(inputFd);
			close(outputFd);
			perror("TEMP FILE OPEN FAIL: ");
                	exit(EXIT_FAILURE);
        	}//CREATE A TEMP FILE TO DON'T OVERWRITE EXISTING DATA
		int numRead;
		while((numRead = read(outputFd, tempBuffer, 160)) > 0){
			if(write(tempFd, tempBuffer, numRead) != numRead)
				exit(-1);
			numberOfBytesToTemp += numRead;
		}
		lseek(outputFd, -(numberOfBytesToTemp), SEEK_CUR);
		if(fcntl(outputFd, F_SETLKW, &writeLock) == -1){
			close(inputFd);
			close(outputFd);
			close(tempFd);
			perror("FILE LOCK SET FAIL: ");
                	exit(EXIT_FAILURE);
		}//SET WRITELOCK ON OUTPUT
		int writeStatus = write(outputFd, lineBuffer, strlen(lineBuffer));
		if(writeStatus != strlen(lineBuffer)){
			close(outputFd);
			close(inputFd);
			close(tempFd);
			perror("OUTPUT FILE WRITE FAIL: ");
	                exit(EXIT_FAILURE);
		}
		if(lseek(tempFd, 1,SEEK_SET) == -1){
			close(outputFd);
                        close(inputFd);
                        close(tempFd);
			perror("TEMP FILE SEEK FAIL: ");
                	exit(EXIT_FAILURE);
		}//WRITE BACK ORIGINAL NUMBERS
		while((numRead = read(tempFd, tempBuffer, 160)) > 0){
                        if(write(outputFd, tempBuffer, numRead) != numRead)
                                exit(-1);
                }
		close(tempFd);
		lock.l_type = F_UNLCK;
		if(fcntl(outputFd, F_SETLKW, &lock) == -1){
			close(outputFd);
                        close(inputFd);
                        close(tempFd);
                        perror("FILE LOCK SET FAIL: ");
                	exit(EXIT_FAILURE);
		}  //UNLOCK FILE
		sleep(sleepTime);
	}
	if(close(inputFd) == -1){
		close(outputFd);
		perror("FILE CLOSE FAIL: ");
                exit(EXIT_FAILURE);
	}
	if(close(outputFd) == -1){
		perror("FILE CLOSE FAIL: ");
                exit(EXIT_FAILURE);
	}
	return EXIT_SUCCESS;
}

void convertComplex(char* twoByte, char** complexBuffer){
	*complexBuffer = (char*) malloc(10 * sizeof(char));
	char axis[4];
	sprintf(axis, "%d", (int)twoByte[0]);
	strcpy(*complexBuffer, axis);
	strcat(*complexBuffer, "+i");
	sprintf(axis, "%d", (int)twoByte[1]);
	strcat(*complexBuffer, axis);
	return;
}
















