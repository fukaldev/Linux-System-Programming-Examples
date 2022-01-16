TARGET=program
CC=gcc
CFLAGS=-lm

programmake: program.c helper.c
	$(CC) program.c helper.c $(CFLAGS) -o $(TARGET)

clean:
	rm -f $(TARGET)