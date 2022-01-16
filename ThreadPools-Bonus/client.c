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
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include "printer.h"

int main(int argc, char *argv[])
{
        char                    c;
        int                     *path;
        int                     size;
        struct sockaddr_in      client;
        int                     aFlag = 0, pFlag = 0, sFlag = 0, dFlag = 0, err = 0;
        char                    *ip;
        int                     port;
        int                     sourceNode, destNode;
        int                     pairNode[2];
        int                     socketFd;
        time_t                  t; 

        while((c = getopt(argc, argv, "a:p:s:d:")) != -1)
        {
                switch (c)
                {
                case 'a':
                        aFlag = 1;
                        ip = optarg;
                        break;
                case 'p':
                        pFlag = 1;
                        port = strtol(optarg, NULL, 10);
                        break;
                case 's':
                        sFlag = 1;
                        sourceNode = strtol(optarg, NULL, 10);
                        break;
                case 'd':
                        dFlag = 1;
                        destNode = strtol(optarg, NULL, 10);
                        break;
                case '?':
			err = 1;
			break;
                }
        }

        if(aFlag == 0 || pFlag == 0 || sFlag == 0 || dFlag == 0 || err)
        {
                print("USAGE: ./client -a [ip] -p [PORT] -s [SourceNode] -d [DestinationNode]\n", STDERR_FILENO, 0);
                _exit(EXIT_FAILURE);
        }

        if(sourceNode < 0 || destNode < 0)
        {
                print("Nodes must be unsigned integer.\n", STDERR_FILENO, 0);
                _exit(EXIT_FAILURE);
        }

        if((socketFd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        {
        
                print("Error while open server socket: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                _exit(EXIT_FAILURE);
        }

        memset(&client, 0, sizeof(client));
        client.sin_family = AF_INET;    //
        client.sin_port = htons(port);  //Define port in struct
        client.sin_addr.s_addr = inet_addr(ip); //Put address to struct

        time(&t);
        print("Client (%d) connecting to %s:%d ======%s", STDOUT_FILENO, 4, getpid(), ip, port, ctime(&t));

        if(connect(socketFd, (struct sockaddr*)&client, sizeof client) == -1) //Connect to server
        {
                print("Error while connect to server socket: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                _exit(EXIT_FAILURE);
        }

        pairNode[0] = sourceNode;
        pairNode[1] = destNode;
        print("Client (%d) connected and requesting a path from node %d to %d ======%s", STDOUT_FILENO, 4, getpid(), sourceNode, destNode, ctime(&t));
        clock_t elapsed = clock();
        
        if(write(socketFd, &pairNode[0], sizeof(int)*2) == -1) //Send nodes as source and destination
        {
                print("Error while writing to server socket: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                _exit(EXIT_FAILURE);
        }

        if(read(socketFd, &size, sizeof(int)) == -1) //Read size of path and allocate size according to this value
        {
                print("Error while reading size of path: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                _exit(EXIT_FAILURE);
        }

        path = malloc(sizeof(int)*size);
        if(!path)
        {
                print("Error while malloc path: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                _exit(EXIT_FAILURE);
        }

        if(read(socketFd, &path[0], sizeof(int)*size) == -1)//Read path to allocated array
        {
                print("Error while reading size of path: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                free(path);
                _exit(EXIT_FAILURE);
        }

        elapsed = clock() - elapsed;
        double timeTake = ((double)elapsed)/CLOCKS_PER_SEC;
        
        if(close(socketFd) == -1)//Close connection
        {
                print("Error while closing socket fd: %s line: %d\n", STDERR_FILENO, 2, strerror(errno), __LINE__);
                free(path);
                _exit(EXIT_FAILURE);
        }

        print("Server’s response to (%d): ", STDOUT_FILENO, 1, getpid());

        int index = 0;
        if(path[index] == -1)//Print path if it is possible, otherwise print No PATH!
        {
                print("No PATH!", STDOUT_FILENO, 0);
        }
        else
        {       while(path[index] != -1)
                {
                        print("%d", STDOUT_FILENO, 1, path[index]);
                        if(path[index+1] != -1)
                                print("->", STDOUT_FILENO, 0);
                        index++;
                }
        }
        
        time(&t);
        print(", arrived in %f second, shutting down ======%s", STDOUT_FILENO, 2, timeTake, ctime(&t));
        free(path); //Free resource
        exit(EXIT_SUCCESS);
}
