CC = gcc
CFLAGS = -Wall -g 

.PHONY: depend clean

all:    server
server: server.o
	$(CC) $(CFLAGS) -o mtserver server.o -lpthread

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o mtserver
