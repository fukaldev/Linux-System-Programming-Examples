#ifndef SEMUN_H
#define SEMUN_H

#include <sys/sem.h>
#include <sys/types.h>

union semun
{
        int val;
        struct semid_ds *buf;
        unsigned short *array;
#if defined(__linux__)
        struct seminfo *__buf;
#endif
};

#endif

