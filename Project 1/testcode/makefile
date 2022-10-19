CC = gcc
CFLAGS = -Wall -g

all : ProtoNum.exe crc.exe header_vals.exe clock_test.exe file_io.exe

ProtoNum.exe : ProtoNum.o
	$(CC) $(CFLAGS) -o ProtoNum.exe ProtoNum.o

ProtoNum.o : ProtoNum.c
	$(CC) $(CFLAGS) -o ProtoNum.o -c ProtoNum.c

crc.exe : crc.c crc_main.c
	$(CC) $(CFLAGS) -o crc.exe crc_main.c crc.c

header_vals.exe : header_vals.c ../shared/defines.h
	$(CC) $(CFLAGS) -o header_vals.exe header_vals.c

clock_test.exe : clock_test.c
	$(CC) $(CFLAGS) -o clock_test.exe clock_test.c

file_io.exe : file_io.c
	$(CC) $(CFLAGS) -o file_io.exe file_io.c

clean : 
	rm -f *.o *.exe