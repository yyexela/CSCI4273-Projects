CC = gcc
CFLAGS = -Wall -g
LIBS = -lpthread

all : client.exe

client.exe : client.o shared.o
	$(CC) $(CFLAGS) -o dfc client.o shared.o $(LIBS)

client.o : client.c client.h ../shared/shared.h
	$(CC) $(CFLAGS) -o client.o -c client.c 

shared.o : ../shared/shared.c ../shared/shared.h
	$(CC) $(CFLAGS) -o shared.o -c ../shared/shared.c

clean : 
	rm -f *.o *.exe client.exe dfc