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

extern sig_atomic_t existence;
extern char* input; 

typedef struct { 
        int x;
        int y;
} point;

typedef struct {
        double theta1;
        double theta0;
} regression;

typedef struct {
        double mae;
        double mse;
        double rmse;
} metrics;

void calculateMetric(double*, double*, double*, int);
int deleteFirstLine(int);
void p1Done(int);
void justReturn(int);
void generalHandler(int);
void termProcess(int);
void calculateAndCreateLine(char* , char*);
void processLine(char* , char*, metrics*);

#endif