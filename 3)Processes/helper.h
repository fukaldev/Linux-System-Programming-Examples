#ifndef HELPER_H
#define HELPER_H

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <math.h>

void multiply(int* quarterA, int* quarterB, int* quarterC, int dimension);
void sum(int* quarterC1, int* quarterC2, int* matrixC, int dimension);


#endif