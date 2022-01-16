#include "functions.h"

double distance(point p1, point p2)
{
        double x = fabs(p1.x-p2.x);
        double y = fabs(p1.y-p2.y);

        if(x>y)
                return x;
        else
                return y;
}

void processFlorist(char *lineBuffer, florist *f)
{
        int index = 0;
        int lineIndex = 0, flowerFind;
        int numOfFlower = 0;
        char buffer [128];
        while(lineBuffer[lineIndex] != ' ')
                buffer[index++] = lineBuffer[lineIndex++];
        buffer[index] = '\0';

        f->floristName = (char*)malloc(sizeof(char)*(index+1));
        strcpy(f->floristName, buffer);

        lineIndex += 2;
        index = 0;

        while(lineBuffer[lineIndex] != ',')
                buffer[index++] = lineBuffer[lineIndex++];
        buffer[index] = '\0';

        f->location.x = strtod(buffer, NULL);

        lineIndex += 1;
        index = 0;

        while(lineBuffer[lineIndex] != ';')
                buffer[index++] = lineBuffer[lineIndex++];
        buffer[index] = '\0';

        f->location.y = strtod(buffer, NULL);


        lineIndex += 2;
        index = 0;

        while(lineBuffer[lineIndex] != ')')
                buffer[index++] = lineBuffer[lineIndex++];
        buffer[index] = '\0';

        f->speed = strtod(buffer, NULL);

        lineIndex += 4;
        index = 0;

        flowerFind = lineIndex;

        while(lineBuffer[flowerFind] != '\0')
                if(lineBuffer[flowerFind++] == ',')
                        numOfFlower++;
        
        f->numberOfFlower = numOfFlower + 1;
        f->flowerTypes = (char**) malloc(sizeof(char*)*(f->numberOfFlower));

        numOfFlower = 0;

        while(numOfFlower < f->numberOfFlower)
        {
                if(numOfFlower != (f->numberOfFlower)-1)
                        while(lineBuffer[lineIndex] != ',')
                                buffer[index++] = lineBuffer[lineIndex++];
                else
                        while(lineBuffer[lineIndex] != '\0')
                                buffer[index++] = lineBuffer[lineIndex++];
                buffer[index] = '\0';
                
                f->flowerTypes[numOfFlower] = (char*)malloc(sizeof(char)*(index+1));
                strcpy(f->flowerTypes[numOfFlower], buffer);
                
                numOfFlower++;
                index = 0;
                lineIndex += 2;
        }

        f->saleStats.totalSelling = 0;
        f->saleStats.totalTime = 0;
}

void processCustomer(char *lineBuffer, customer *c)
{
        int index = 0;
        int lineIndex = 0;
        char buffer [128];
        while(lineBuffer[lineIndex] != ' ')
                buffer[index++] = lineBuffer[lineIndex++];
        buffer[index] = '\0';

        c->customerName = (char*) malloc(sizeof(char)*(index+1));
        strcpy(c->customerName, buffer);

        lineIndex += 2;
        index = 0;

        while(lineBuffer[lineIndex] != ',')
                buffer[index++] = lineBuffer[lineIndex++];
        buffer[index] = '\0';

        c->location.x = strtod(buffer, NULL);

        lineIndex += 1;
        index = 0;

        while(lineBuffer[lineIndex] != ')')
                buffer[index++] = lineBuffer[lineIndex++];
        buffer[index] = '\0';

        c->location.y = strtod(buffer, NULL);


        lineIndex += 3;
        index = 0;

        while(lineBuffer[lineIndex] != '\0')
                buffer[index++] = lineBuffer[lineIndex++];
        buffer[index] = '\0';

        c->order = (char*) malloc(sizeof(char)*(index+1));
        strcpy(c->order, buffer);

}

int print(const char* str, int output, int argNum, ...) //Print function
{
        char* line;
        va_list argList;
        va_start(argList, argNum);
        int size = vsnprintf(NULL, 0, str, argList);
        va_end(argList);

        line = (char*)malloc(sizeof(char)*(size+1));
        if(!line)
        {
                write(STDERR_FILENO, "print line allocation error\n", 28);
                exit(EXIT_FAILURE);
        }
        va_start(argList, argNum);
        if(vsnprintf(line, size+1, str, argList) <= 0)
        {
                free(line);
                write(STDERR_FILENO, "format output error\n", 20);
                exit(EXIT_FAILURE);
        }

        if(write(output, line, size) == -1)
        {
                free(line);
                write(STDERR_FILENO, "print write error\n", 18);
                exit(EXIT_FAILURE);
        }
        free(line);
        return size;
}

int inStock(florist f, char *flowerType)
{
        for(int i = 0; i < f.numberOfFlower; i++)
                if(strcmp(f.flowerTypes[i], flowerType) == 0)
                        return 1;
        
        return 0;
}