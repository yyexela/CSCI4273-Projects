#ifndef SERVER_H
#define SERVER_H

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#define MAX_REQ             8192    // Max text line length for request
#define MAX_RES             8192    // Max text line length for response
                                    // (also used as max HTTP header size from HTTP server, keep as at least 8192)
#define MAXBUF              8192    // Max I/O buffer size
#define LISTENQ             1024    // Second argument to listen()
#define MAX_METH            16      // Max acceptable HTTP request method length
#define MAX_PORT            8       // Max acceptable port length
#define MAX_URI             2048    // Max acceptable URI length
#define MAX_HOST            2048    // Max acceptable hostname length
#define MAX_VER             16      // Max acceptable HTTP request length
#define MAX_EXT             16      // Max acceptable extension size
#define MAX_STATUS          16      // Max acceptable status code size in digits
#define MAX_FILE_SZ         128     // Max acceptable file size in digits
#define MAX_FILE_TYPE_SZ    25      // Max file type size buffer
#define MAX_WRITE_SZ        4096    // Max number of bytes send per `write`, got from the minimum value in '/proc/sys/net/ipv4/tcp_wmem'
#define MAX_CONTENT_SZ      16      // Max number of bytes allowed for content-length value (as a string)

#define CONN_TIMEOUT        60      // Timeout, in seconds, of the connection

#define PRINTS              0       // 1: enable all print statements, 0: disable all print statements

#define PRINT_REQ_TOTAL     0       // 1: enable this print statement, 0: disable this print statement
#define PRINT_REQ_HDR_INFO  0       // v
#define PRINT_FILE_SIZE     0
#define PRINT_RES_HDR       0
#define PRINT_CONN_INFO     0
#define PRINT_EXIT_INFO     0
#define PRINT_DIVIDERS      0
#define PRINT_LABELS        0
#define PRINT_BYTES_READ    0
#define PRINT_WRITE_DONE    0
#define PRINT_TIMEOUT       0
#define PRINT_DNS_RESULTS   0
#define PRINT_SERVER_CONN   0
#define PRINT_MD5           0
#define PRINT_CONTENT_SZ    0
#define PRINT_SEND_INFO     0
#define PRINT_WRITE_INFO    0
#define PRINT_CACHE_INFO    0
#define PRINT_BLACKLIST     0

// Used for storing cache time
typedef struct node{
    struct timespec cache_time;
    char md5[39];
    struct node *next;
} node;

// Check command line arguments
int init_cmdline(int argc, char * argv[], int * port, time_t * cache_timeout);

// Bind to a socket as the server
int open_listenfd(int port);

// Given a connection file_descriptor, return a basic HTML page
void hardcoded_response(int conn_fd);

// Main function for threads created
void *thread_main(void *vargp);

// Given a connection file descriptor, extract the request method, uri, and version (req_* should be set to 0-bytes first)
// NOTE: the uri is prepended with "www" in order to work with path searches on the server
int extract_req_fields(char *req_buf, char *req_meth, char *req_uri, char *req_ver, char *req_host, char *req_port);

// Process a get request
int handle_req(int conn_fd, char *req_uri, char *req_ver, char *req_host, char *req_port, char *hdr);

// Given a connection file descriptor, read at most n bytes into the buffer char *buf
ssize_t read_conn_fd(int *conn_fd, char *buf, int n);

// Given a req_uri (URI) and file_ext, fill file_ext with the extension of the file in req_uri
int extract_file_extension(char *req_uri, char  *file_ext);

// Given a valid open file, return its size or -1 on error
uint32_t get_file_size(FILE * file);

// Populate 'header' with the appropriate header content of an HTTP response
int make_header(char *header, char *req_ver, int status, char * type, uint32_t file_sz, int if_post, int post_len);

// Send the header and file contents
int send_data(int conn_fd, char *header, FILE *file, uint32_t file_sz);

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

// Upon an error, send an HTTP error header to the socket conn_fd with error number `status`
int send_error(int conn_fd, int status, char *req_ver);

// Create a header with the status code provided
int make_error_header(char *header, int status, char *req_ver);

// Check if hostname is resolved by DNS, save result in req_ip
int check_hostname(char *req_host, char *req_ip);

// Open up a TCP socket at `server_fd`
int init_server_tcp(int *server_fd, struct addrinfo *hints, struct addrinfo *serverinfo, char *req_host, char *req_port);

// Stores the 32 bit + '\0' string of the md5 sum in `ret` from the string given by `req_uri`
int get_md5(char *req_uri, char *ret);

// Extract 'Content-Length: ' header value from 'buf'
int get_content_length(char *buf, char *ret);

// Receive a file from the socket `server_fd`. `filename` saves the name of the file
int recv_file(int *server_fd, FILE *file, char *req_host, char *req_port, int *file_size);

// Place size of header from `buf` into `n`
int get_header_size(char *buf, uintptr_t *n);

// Given a TCP connection, a buffer, the maximum number of things that can be put in the buffer, and the size of the buffer,
// fill the buffer with a complete header (up to and including \r\n\r\n)
// saving at least one '\0' at the end
ssize_t obtain_header(int *conn_fd, char *buf, int max_buf);

// Print the names of the items in the linked list
void print_ll();

// Create an empty node, returns the memory value of the malloc'd pointer
node * create_node();

// Given a node *, update its time to the current time
void update_time(node *ptr);

// Given a node *, update its name (md5)
void update_name(node *ptr, char *name);

// Add node to head of linked list
// (more recently created files are more likely to be accessed)
void add_node(node *ptr);

// Given a node *, delete it from the linked list freeing memory
void delete_node(node *ptr);

// Given an md5 name, return the node * in the LL or NULL if it's not there
node * check_node(char *name);

// Given a name, check if its timeout has expired (1) , (0) otherwise
int check_timeout(node *ptr);

// Given a host_ip and a host_name, save the result in resolved.txt
int save_ip(char *host_name, char *host_ip);

// Given a host_name, save the host_ip to host_ip of size MAX_URI
int get_ip(char *host_name, char *host_ip);

// Check if the desired host name or IP address is blocked
// Returns 1 if blacklisted, 0 otherwise
int check_blacklist(char* hostname, char* ip);

#endif