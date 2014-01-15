CC = gcc
CFLAGS = -Wall -g 

.PHONY: depend clean

all:    server
server: server.o
	$(CC) $(CFLAGS) -o server server.o -lpthread

# this is a suffix replacement rule for building .o's from .c's
# it uses automatic variables $<: the name of the prerequisite of
# the rule(a .c file) and $@: the name of the target of the rule (a .o file) 
# (see the gnu make manual section about automatic variables)
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o server

depend: $(SRCS)
	makedepend $(INCLUDES) $^
