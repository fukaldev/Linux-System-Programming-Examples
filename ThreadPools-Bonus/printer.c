#include "printer.h"

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