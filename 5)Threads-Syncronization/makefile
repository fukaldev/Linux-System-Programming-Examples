TARGET=floristApp
CC=gcc
CFLAGS=-Wall -lm -pthread -g

programmake: floristApp.c functions.c
	$(CC) floristApp.c functions.c $(CFLAGS) -o $(TARGET)

clean:
	rm -f $(TARGET)