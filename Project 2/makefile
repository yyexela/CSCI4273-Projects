CC = gcc
CFLAGS = -Wall -g
LIBS = -lpthread

all : server.exe

server.exe : server.c server.h
	$(CC) $(CFLAGS) -o server.exe server.c server.h $(LIBS)

clean : 
	rm -f *.o *.exe