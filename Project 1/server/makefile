CC = gcc
CFLAGS = -Wall -g
OBJECTS = server.o

all : crc.exe server.exe

server.exe : crc.o server.o funcs.o
	$(CC) $(CFLAGS) -o server.exe crc.o server.o funcs.o

server.o : uftp_server.c server.h
	$(CC) $(CFLAGS) -o server.o -c uftp_server.c

crc.exe : crc.o crc_main.o
	$(CC) $(CFLAGS) -o crc.exe crc.o crc_main.o

crc.o : ../shared/crc.c ../shared/crc.h
	$(CC) $(CFLAGS) -o crc.o -c ../shared/crc.c

crc_main.o : ../shared/crc_main.c ../shared/crc.h
	$(CC) $(CFLAGS) -o crc_main.o -c ../shared/crc_main.c

funcs.o : ../shared/funcs.c ../shared/funcs.h
	$(CC) $(CFLAGS) -o funcs.o -c ../shared/funcs.c

clean : 
	rm -f *.o *.exe foo*