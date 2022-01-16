#ifndef STRUCTURES_H
#define STRUCTURES_h


typedef struct
{
        double x;
        double y;
} point;

typedef struct
{
        int totalSelling;
        unsigned int totalTime;
} stats;

typedef struct
{
        int id;
        char *floristName;
        int numberOfFlower;
        char **flowerTypes;
        point location;
        double speed;
        stats saleStats;
} florist;

typedef struct
{
        char *customerName;
        point location;
        char *order;
} customer;

 

#endif