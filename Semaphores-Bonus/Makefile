TARGET=program
CC=gcc
CFLAGS= -pthread -lrt

programmake: program.c
	$(CC) program.c $(CFLAGS) -o $(TARGET)

clean:
	rm -f $(TARGET)