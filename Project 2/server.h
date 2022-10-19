#ifndef SERVER_H
#define SERVER_H

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_REQ             8192    // Max text line length for request
#define MAX_RES             8192    // Max text line length for response
#define MAXBUF              8192    // Max I/O buffer size
#define LISTENQ             1024    // Second argument to listen()
#define MAX_METH            16      // Max acceptable HTTP request method length
#define MAX_URI             2048    // Max acceptable URI length
#define MAX_VER             16      // Max acceptable HTTP request length
#define MAX_EXT             16      // Max acceptable extension size
#define MAX_STATUS          16      // Max acceptable status code size in digits
#define MAX_FILE_SZ         128     // Max acceptable file size in digits
#define MAX_FILE_TYPE_SZ    25      // Max file type size buffer
#define MAX_WRITE_SZ        4096    // Max number of bytes send per `write`, got from the minimum value in '/proc/sys/net/ipv4/tcp_wmem'

#define CONN_TIMEOUT        10      // Timeout, in seconds, of the connection

#define PRINTS              1       // 1: enable print statement, 0: disable print statements
#define PRINT_REQ_TOTAL     0       
#define PRINT_REQ_HDR_INFO  0       
#define PRINT_FILE_SIZE     0
#define PRINT_HOME_CHANGE   0
#define PRINT_RES_HDR       0
#define PRINT_CONN_INFO     1
#define PRINT_EXIT_INFO     0
#define HARDCODED_RES       0
#define PRINT_DIVIDERS      0
#define PRINT_LABELS        0
#define PRINT_BYTES_READ    0
#define PRINT_WRITE_DONE    0
#define PRINT_TIMEOUT       1

// Bind to a socket as the server
int open_listenfd(int port);

// Given a connection file_descriptor, return a basic HTML page
void hardcoded_response(int conn_fd);

// Main function for threads created
void *thread_main(void *vargp);

// Given a connection file descriptor, extract the request method, uri, and version (req_* should be set to 0-bytes first)
// NOTE: the uri is prepended with "www" in order to work with path searches on the server
int extract_req_fields(char *req_buf, char *req_meth, char *req_uri, char *req_ver);

// Process a get request
int handle_req(int conn_fd, char *req_uri, char *req_ver, int if_post, char *post_buf, int post_len);

// Given a connection file descriptor, read at most n bytes into the buffer char *buf
ssize_t read_conn_fd(int conn_fd, char *buf, int n, struct timespec *req_time, int use_timout);

// Given a req_uri (URI) and file_ext, fill file_ext with the extension of the file in req_uri
int extract_file_extension(char *req_uri, char  *file_ext);

// Given a valid open file, return its size or -1 on error
uint32_t get_file_size(FILE * file);

// Populate 'header' with the appropriate header content of an HTTP response
int make_header(char *header, char *req_ver, int status, char * type, uint32_t file_sz, int if_post, int post_len);

// Send the header and file contents as a response
int send_response(int conn_fd, char *header, FILE *file, uint32_t file_sz, int if_post, char* post_buf, int post_len);

// Handle the SIGPIPE error when writing into a closed socket
void sigpipe_handler(int signum);

// Write to socket catching errors when the socket is closed (returns 0), other errors (returns -1), and the bytes written (returns bytes)
int clean_write(int fd, void *buf, size_t n);

// sets hdr to a char * to the beginning of the next HTTP command in req_buf, NULL otherwise
// also returns hdr
char *get_next_header(char *req_buf, char *hdr);

// Returns 1 if "Connection: keep-alive" is found in current header, 0 otherwise
// char *hdr is the pointer to the beginning of the HTTP request
int get_keep_alive(char *hdr);

#endif