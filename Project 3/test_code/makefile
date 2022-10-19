CC = gcc
CFLAGS = -Wall -g
LIBS = -lpthread

all : packet_parse.exe extract_test.exe get_md5.exe read_file.exe linked_list.exe

linked_list.exe : linked_list.c
	$(CC) $(CFLAGS) -o linked_list.exe linked_list.c $(LIBS)

read_file.exe : read_file.c
	$(CC) $(CFLAGS) -o read_file.exe read_file.c $(LIBS)

get_md5.exe : get_md5.c
	$(CC) $(CFLAGS) -o get_md5.exe get_md5.c $(LIBS)

packet_parse.exe : packet_parse.c
	$(CC) $(CFLAGS) -o packet_parse.exe packet_parse.c $(LIBS)

extract_test.exe : extract_test.c
	$(CC) $(CFLAGS) -o extract_test.exe extract_test.c $(LIBS)

clean : 
	rm -f *.o *.exe