/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
// For open()
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
// CRC
#include "../shared/crc.h"
// #define's
#include "../shared/defines.h"
// Shared funcctions for packet manipulation
#include "../shared/funcs.h"
// Server-specific functions
#include "server.h"

// Check and initialize command line arguments
int init_cmdline(int argc, char * argv[], char ** port_no) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    return -1;
  }
  if (atoi(*port_no) <= 5000){
    fprintf(stderr, "<port> must be greater than 5000, not %s\n", *port_no);
    return -1;
  }
  
  return 0;
}

// Initialize hints struct
int init_hints(struct addrinfo * hints){
  memset((char *) hints, 0, sizeof(struct addrinfo)); // Set the struct to all 0's first
  hints->ai_family = AF_INET; // Use only IPV4
  hints->ai_socktype = SOCK_DGRAM; // UDP
  hints->ai_flags = AI_PASSIVE; // Needed for server to call 'bind' and 'accept'
  return 0;
}

// Initialize the socket
int init_socket(struct addrinfo * server_infos, struct addrinfo ** p, int * sock_fd, int * yes){

  // Looped through linked list from getaddrinfo and bind to first socket we can
  for(*p = server_infos; *p != NULL; *p = (*p)->ai_next){
    // Try creating socket
    if((*sock_fd = socket(server_infos->ai_family, server_infos->ai_socktype, server_infos->ai_protocol)) < 0){
      perror("ERROR creating socket\n");
      continue; // Try next struct in server_infos
    }

    // Set to non-blocking
    if(fcntl((*sock_fd), F_SETFL, O_NONBLOCK) == -1){
      perror("ERROR setting non-block socket\n");
      return -1;
    }

    // Allow the socket to be reused
    if(setsockopt(*sock_fd, SOL_SOCKET, SO_REUSEADDR, yes, sizeof((*yes))) < 0){
      close(*sock_fd); // Release the socket
      perror("ERROR setting socket option to reuse address\n");
      continue; // Try next struct in server_infos
    }

    // Try binding to the socket
    if(bind(*sock_fd, server_infos->ai_addr, server_infos->ai_addrlen) < 0){
      close(*sock_fd); // Release the socket
      perror("ERROR binding socket\n");
      continue; // Try next struct in server_infos
    }

    break;
  }

  // Check if we have successfully bound to a socket
  if(p == NULL){
    fprintf(stderr, "ERROR at binding to a socket\n");
    close(*sock_fd);
    freeaddrinfo(server_infos);
    return -1;
  }

  return 0;
}

int run_cmd(char *cmd){
  FILE *cmd_response;
  int cmd_length = strlen(cmd);

  if(cmd_length < MAX_PAYLOAD_SZ - 25){
    strncpy(cmd + cmd_length, " > cmd_response_temp.txt", 25);
  }
  else{
    printf("ERROR, command length too long, aborting command");
    return -1;
  }

  if((cmd_response = popen(cmd, "r")) == NULL){
      printf("ERROR calling \"popen\" with command \"ls\"\n");
      return -1;
  }

  if(pclose(cmd_response)){
    printf("ERROR with pclose\n");
  }
  return 0;
}

int main(int argc, char **argv) {
  int sock_fd; // Socket file descriptor
  int rv; // Return value used in error printg
  char * port_no = argv[1]; // String port number
  char send_buf[MAX_PACKET_SZ]; // Where the payload will be sent from
  char recv_buf[MAX_PACKET_SZ]; // Where the payload will be received
  char user_buf[MAX_USER_BUF]; // Where we'll store file name if needed
  int yes = 1; // Used to clear port if in use
  struct addrinfo hints, *server_infos, *p; // Used in networking
  struct sockaddr_storage in_info; // Stores information of connected person
  socklen_t from_len = sizeof(struct sockaddr_storage); // Used in recv function
  connection_info info;

  // Check command line arguments 
  if (init_cmdline(argc, argv, &port_no) < 0)
    exit(1);

  // Initialize the addrinfo (hints) struct
  if (init_hints(&hints) < 0)
    exit(1);

  // getaddrinfo: get address information of server
  if((rv = getaddrinfo(NULL, port_no, &hints, &server_infos)) < 0){
    fprintf(stderr, "ERROR initializing address/port number combo: %s\n", gai_strerror(rv));
    exit(1);
  }

  // Initialize the socket
  if(init_socket(server_infos, &p, &sock_fd, &yes) < 0)
    exit(1);

  // Initialize CRC lookup table
  F_CRC_InicializaTabla();

  // Main loop
  while(1){
    // Zero out receive buffer to be safe
    clear_packet(recv_buf);

    // Listen for a message from the client and populate in_info with sender info
    while(recvfrom(sock_fd, recv_buf, MAX_PACKET_SZ, 0, (struct sockaddr *) &in_info, &from_len) == -1){
      if(!(errno == EAGAIN || errno == EWOULDBLOCK)){
        // Error isn't a result of non-blocking, actual error
        perror("ERROR with recvfrom");
        printf("errno %u\n", errno);
        return -1;
      }
    }

    info.sock_fd = sock_fd;
    info.addr = (struct sockaddr *) &in_info;
    info.addrlen = from_len;
    info.in_info = &in_info;
    info.from_len = from_len;
    info.send_buf = send_buf;
    info.recv_buf = recv_buf;

    // Print IP and Port of message
    if(PRINT_PACKET_IP_PORT) print_ip_and_port(&in_info);
    
    // Self explanatory
    if(PRINT_PACKET_HDRS){
      print_packet_headers(recv_buf);
      printf("\n");
    }

    // Check packet CRC
    if (check_packet_crc(recv_buf) < 0)
      continue; //Wait to get another packet

    // Check the type of packet we have received (looking for probe packet)
    uint8_t flags = get_flags((char *) recv_buf);

    // Probe message
    if(flags == 0x12){
      // We will be receiving a file
      if (get_filename(recv_buf, (char *) user_buf) < 0){
        printf("ERROR in get_file_name, skipping rest of the transmission\n");
        continue;
      }
      printf("File name to receive: %s\n", user_buf);
      uint32_t packets_total = get_packet_no(recv_buf);

      prep_packet(&info, 0, 0x12, 0, NULL);
      send_packet(info.sock_fd, info.send_buf, info.addr, info.addrlen);

      if(packets_total == 0){
        printf("File not found on client, accepting new commands...\n");
        continue;
      } 

      // Start receiving the file
      if ((rv = recv_file(&info, user_buf, packets_total)) < 0){
        if(rv == -2){
          printf("Expected data packet was a probe packet, aborting command...\n");
          continue;
        }
        printf("ERROR in recv_file\n");
        cleanup(sock_fd, server_infos, 1);
      }
    }
    else if (flags == 0x22){
      // Server will send a file
      char filename[MAX_PAYLOAD_SZ];
      get_filename(recv_buf, filename);

      prep_packet(&info, 0, 0x22, 0, NULL);
      send_packet(info.sock_fd, info.send_buf, info.addr, info.addrlen);

      if(send_file(&info, 0x22, filename) < 0){
        printf("ERROR in send_file\n");
        cleanup(sock_fd, server_infos, 1);
      }
    } 
    else if (flags == 0x32){
      // Extract command
      char cmd_in[MAX_PAYLOAD_SZ]; // Where we store the command we're running
      memset(cmd_in, 0, MAX_PAYLOAD_SZ);
      strncpy(cmd_in, recv_buf + PAYLOAD_OFFSET, strlen(recv_buf + PAYLOAD_OFFSET));

      run_cmd(cmd_in);

      prep_packet(&info, 0, 0x32, 0, NULL);
      send_packet(info.sock_fd, info.send_buf, info.addr, info.addrlen);

      if(send_file(&info, 0x32, "cmd_response_temp.txt") < 0){
        printf("ERROR in send_file\n");
        cleanup(sock_fd, server_infos, 1);
      }

      pclose(popen("rm cmd_response_temp.txt", "r"));
    }
    else if(flags == 0x04){
      printf("Received exit command, closing socket and ending program...\n");
      cleanup(sock_fd, server_infos, 0);
    }

  }

  // Clean up
  cleanup(sock_fd, server_infos, 1);
}
