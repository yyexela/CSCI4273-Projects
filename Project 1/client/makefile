CC = gcc
CFLAGS = -Wall -g
OBJECTS = 

all : crc.exe client.exe

client.exe : crc.o client.o funcs.o
	$(CC) $(CFLAGS) -o client.exe crc.o client.o funcs.o

client.o : uftp_client.c client.h
	$(CC) $(CFLAGS) -o client.o -c uftp_client.c

crc.exe : crc.o crc_main.o
	$(CC) $(CFLAGS) -o crc.exe crc.o crc_main.o

crc.o : ../shared/crc.c ../shared/crc.h
	$(CC) $(CFLAGS) -o crc.o -c ../shared/crc.c

crc_main.o : ../shared/crc_main.c ../shared/crc.h
	$(CC) $(CFLAGS) -o crc_main.o -c ../shared/crc_main.c

funcs.o : ../shared/funcs.c ../shared/funcs.h
	$(CC) $(CFLAGS) -o funcs.o -c ../shared/funcs.c

clean : 
	rm -f *.o *.exe