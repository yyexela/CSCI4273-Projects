#ifndef CLIENT_H
#define CLIENT_H

#include <stdint.h>
#include <netdb.h>
#include <time.h>

// Check command line arguments
int init_cmdline(int argc, char * argv[], char ** hostname, char ** port_no);

// Initialize the hints
int init_hints(struct addrinfo * hints);

// Initialize the socket
int init_socket(struct addrinfo * server_info, struct addrinfo ** p, int * sock_fd);

// Input a message string and put it into the packet 'buf' with at most 'bytes' bytes
void get_message(uint16_t * bytes_no, char * buf, int bytes);

#endif