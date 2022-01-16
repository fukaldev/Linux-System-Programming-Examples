#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "structures.h"

double distance(point p1, point p2);
int print(const char* str, int output, int argNum, ...);
void processFlorist(char *lineBuffer, florist *f);
void processCustomer(char *lineBuffer, customer *c);
int inStock(florist f, char *flowerType);


#endif