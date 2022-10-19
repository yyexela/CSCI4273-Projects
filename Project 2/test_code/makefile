CC = gcc
CFLAGS = -Wall -g
LIBS = -lpthread

all : packet_parse.exe timer.exe

packet_parse.exe : packet_parse.c
	$(CC) $(CFLAGS) -o packet_parse.exe packet_parse.c $(LIBS)

timer.exe : timer.c
	$(CC) $(CFLAGS) -o timer.exe timer.c $(LIBS)

clean : 
	rm -f *.o *.exe