#include "helper.h"

//Multiple given quarters and assign values to result quarter
void multiply(int* quarterA, int* quarterB, int* quarterC, int dimension)
{
        for(int i = 0; i < dimension; i++)
        {
                for(int j = 0; j < dimension; j++)
                {
                        int sum = 0;
                        for(int k = 0; k < dimension; k++)
                                sum += quarterA[(i*dimension)+k] * quarterB[(k*dimension)+j];
                        quarterC[(i*dimension)+j] = sum;
                }
        }
}

//Sum given quarters and conclude final result
void sum(int* quarterC1, int* quarterC2, int* matrixC, int dimension)
{
        for(int i = 0; i < dimension; i++)
                for(int j = 0; j < dimension; j++)
                        matrixC[(i*dimension)+j] = quarterC1[(i*dimension)+j]+quarterC2[(i*dimension)+j];
}