TARGET1=server
TARGET2=client
CC=gcc
CFLAGS=-Wall -pthread -g

all: server client

server: server.c printer.c threadPool.c dataStructures.c
	$(CC) server.c printer.c threadPool.c dataStructures.c $(CFLAGS) -o $(TARGET1)

client: client.c printer.c
	$(CC) client.c printer.c $(CFLAGS) -o $(TARGET2)
clean:
	rm -f $(TARGET1)
	rm -f $(TARGET2)