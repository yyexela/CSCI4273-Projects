#ifndef CLIENT_H
#define CLIENT_H


#include <stdio.h>      // For file IO
#include <sys/types.h>  // For TCP/Networking
#include <sys/socket.h> 
#include <netdb.h>      
#include "../shared/shared.h"

// Single struct to store server info
typedef struct Server{
    char port[MAX_PORT];
    char ip[MAX_IP];
    char dir[MAX_DIR];
} Server;

// Used for figuring out which files have all parts during query
typedef struct node{
    char filename[MAX_LINE];
    int part[4];
    struct node *next;
} node;

// Seek to the beginning of the part_no segment
int seek_to(FILE * file, int part_sizes[4], int part_no);

// Split filesize into 4 equal parts
int get_part_sizes(int sizes[4], int filesize);

// Stores in `ret` the 32 bit + '\0' string of the md5 from the file given by `req_uri`
int get_md5(char *req_uri, char *ret);

// Send the header and file contents
int send_part(Server * servers, char *filename, int part_sizes[4], int part_no, int server_no, FILE *file);

// Close TCP connections and free serverinfos
int close_connections(int * server_fd, struct addrinfo ** serverinfo);

// Check and initialize command line arguments
int init_cmdline(int argc, char * argv[], Server * servers);

// Zero the values in servers
void zero_servers(Server * servers);

// Given a line from dfc.conf, fills in the server struct
int update_server_info(Server * server, char * line, char * argv[]);

// Prints the client-side menu
void print_menu();

// Input a message string and put it into the packet 'buf' with at most 'bytes' bytes
void get_message(char * buf, int bytes);

// Handles sigint
void sigint_handler(int signum);

// Runs the client-side "put" command
void run_put(Server * servers, char * cmd);



// Tries to initiate a TCP connection with the server
int init_server_tcp(int *server_fd, struct addrinfo *serverinfo, 
                    char *req_host, char *req_port);

// Ask each server which files it has and figure out if we have complete files
// If is_get is true, fetch the deignated file from the servers
void run_list_get(Server * servers, int is_get, char * cmd);

// Send a query to a server and append to the file it's response
int get_query(Server * servers, int server_no, FILE * file);

// Go through the query file and print all files and their completeness (if !is_get)
// Go through the query file and fetch each part of the designated filename (if is_get)
void parse_query(FILE * file, int is_get, char * filename, Server * servers);

// Requests and downloads a complete file segment from a server
int fetch_piece(Server * servers, char * filename, int piece, char * server_dir);

// Receives a requested file segment from a server
int recv_get_response(int server_fd, char * server_dir, char * filename);

// Parses through retrieved files checking if we have a complete file, if so
// call cat from shell and redirect output to requested file
int compile_get(Server * servers, char * filename);

// Helped function to make file paths for file segments
void make_path(char * path_buf, Server * servers, int server_no, int piece_no);

// Print linked list after it has been constructed
void print_ll(node *head);

// Update the name of a node
void update_name(node *ptr, char *name);

// Malloc a node and set it up
node * create_node();

// Add a node to a linked list
void add_node(node **head, node *ptr);

// Delete a node from a linked list
void delete_node(node **head, node *ptr);

// Check if a node exists in the linked list
node * check_node(node * head, char *filename);

// Delete the entire linked list
void delete_ll(node * head);

// Try connecting to a server with a timeout of 1 second
int tcp_timeout_connect(int *server_fd_ptr, struct addrinfo *serverinfo, char *server_ip, char *server_port);

// Read response into file
int read_into_file(int server_fd, struct addrinfo *serverinfo, FILE * file);

// Receive a file through connection conn_fd
int recv_file(int conn_fd, char *buf, char *dir, int bytes_read);

// Extracts metadata, returns NULL on error and the beginning of the data from buf on success
char * get_put_data(char *buf, char *filename, int *filesize, int *part_no, int *metabytes);

// Specialized function in recv_file
// Write the file data we read during the first call to read into the file
// then keep reading from the socket if there's more data to put in the file
int write_put_data(int conn_fd, FILE * file, char *buf, int bytes_read, int metabytes, int bytes_left);

#endif