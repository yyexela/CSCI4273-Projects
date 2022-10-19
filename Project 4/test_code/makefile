CC = gcc
CFLAGS = -Wall -g
LIBS = -lpthread

all : DFS_folders.exe

#<name>.exe : <name>.c
#	$(CC) $(CFLAGS) -o <name>.exe <name>.c $(LIBS)

DFS_folders.exe : DFS_folders.c
	$(CC) $(CFLAGS) -o DFS_folders.exe DFS_folders.c $(LIBS)

clean : 
	rm -f *.o *.exe