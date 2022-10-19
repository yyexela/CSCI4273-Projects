CC = gcc
CFLAGS = -Wall -g
LIBS = -lpthread

all : server.exe

server.exe : server.o shared.o
	$(CC) $(CFLAGS) -o dfs server.o shared.o $(LIBS)

server.o : server.c server.h ../shared/shared.h
	$(CC) $(CFLAGS) -o server.o -c server.c

shared.o : ../shared/shared.c ../shared/shared.h
	$(CC) $(CFLAGS) -o shared.o -c ../shared/shared.c

clean : 
	rm -f *.o *.exe server.exe dfs