#ifndef FUNCS_H
#define FUNCS_H

// This file contains shared function prototypes for client.c and server.c

#include <stdio.h>
#include <stdint.h> // Used for uintxx_t types
#include <netdb.h> 
#include <errno.h>
#include <time.h>
#include "defines.h"

typedef struct{
    int sock_fd;
    struct sockaddr *addr;
    socklen_t addrlen;
    struct sockaddr_storage *in_info;
    socklen_t from_len;
    char *send_buf;
    char *recv_buf;
} connection_info;

// Wrapper for perror
void error(char * msg);

// Given a packet defined in 'defines.h', return the included crc value
uint32_t get_crc(char * packet);

// Given a packet defined in 'defines.h', return the included packet_no value
uint32_t get_packet_no(char * packet);

// Given a packet defined in 'defines.h', return the included packets_total value (probe packets only)
uint32_t get_packets_total(char * packet);

// Given a packet defined in 'defines.h', return the included bytes_no value
uint16_t get_bytes_no(char * packet);

// Given a packet defined in 'defines.h', return the included flags value
uint8_t get_flags(char * packet);

// Given a packet defined in 'defines.h', set the crc value
void set_crc(char * packet, uint32_t crc);

// Given a packet defined in 'defines.h', set the packet_no value
void set_packet_no(char * packet, uint32_t packet_no);

// Given a packet defined in 'defines.h', set the packets_total value (probe packets only)
void set_packets_total(char * packet, uint32_t packets_total);

// Given a packet defined in 'defines.h', set the bytes_no value
void set_bytes_no(char * packet, uint16_t bytes_no);

// Given a packet defined in 'defines.h', set the flags value
void set_flags(char * packet, uint8_t flags);

// Given a packet defined in 'defines.h', set all the bits to 0
void clear_packet(char * packet);

// Fire a UDP packet
void send_packet(int sock_fd, char * buf, struct sockaddr * addr, socklen_t addr_len);

// Print packet headers
void print_packet_headers(char * buf);

// Print IP and Port number of the input sockaddr_storage
void print_ip_and_port(struct sockaddr_storage * in_info);

// Given a packet, ensure CRC values match
int check_packet_crc(char * buf);

// Send an ACK packet
int send_ack(connection_info *info, uint32_t packet_no);

// Get filename from probe packet
int get_filename(char * recv_buf, char * filename);

// Free memory, close socket, and exit
void cleanup(int sock_fd, struct addrinfo *server_infos, int exitcode);

// Input various header information and payload to assemble the packet
int prep_packet(connection_info *info, uint16_t bytes_no, uint8_t flag, uint32_t packet_no, char *payload);

// Get a packet until you get an uncorrupted one with the correct flags before you time out
int recv_packet(connection_info *info, uint8_t expected_flag, clock_t timeout);

// Send then confirm acknowledgement packet
clock_t send_then_confirm_ack(connection_info *info, uint16_t bytes_no, uint8_t send_flag, uint8_t ack_flag, uint32_t packet_no, char *payload, clock_t timeout);

// Wrapper function to execute the retreival of a file :()
int recv_file(connection_info *info, char *filename, uint32_t packets_total);

// Wrapper function to execute the sending of a file :()
int send_file(connection_info *info, uint8_t flag, char *filename);

// As a client send a command for the server to execute :()
int request_cmd(connection_info *info, char *command);
#endif