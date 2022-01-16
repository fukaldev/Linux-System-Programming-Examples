TARGET=program
CC=gcc
CFLAGS= -pthread -Wall -Wextra -pedantic -g  -I .

programmake: program.c
	$(CC) program.c $(CFLAGS) -o $(TARGET)

clean:
	rm -f $(TARGET)