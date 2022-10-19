#ifndef CLIENT_H
#define CLIENT_H

#include "../shared/shared.h"
#include <sys/types.h> // For size_t

// Single struct to store server info as a parameter
typedef struct Info{
    char port[MAX_PORT];
    char dir[MAX_DIR];
    int conn_fd;
} Info;

// Check and initialize command line arguments
int init_cmdline(int argc, char * argv[]);

// Bind to a socket as the server
int open_listenfd(int port);

// Main thread function
void *thread_main(void *vargp);

// Receive a file through connection conn_fd
int recv_file(int conn_fd, char *buf, char *dir, int bytes_read);

// Extracts metadata, returns NULL on error and the beginning of the data from buf on success
char * get_put_data(char *buf, char *filename, int *filesize, int *part_no, int *metabytes);

// Returns data about files
int return_query(int conn_fd, char *dir);

// Specialized function in recv_file
// Write the file data we read during the first call to read into the file
// then keep reading from the socket if there's more data to put in the file
int write_put_data(int conn_fd, FILE * file, char *buf, int bytes_read, int metabytes, int bytes_left);

// On the server side, we use this as easily editable code to send a fake put request back to the client
// which uses recycled server code to receive after a put
int run_put(int conn_fd, char * cmd, char * dir, int bytes_read);

// Extracts relavent data from the get request that is necessary to send the file back
char * get_get_data(char *buf, char *filename, int *part_no, int * metabytes);

#endif