/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */

// For networking
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <arpa/inet.h> 
#include <errno.h>
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
// Client-only prototypes
#include "client.h"
// For clock() (time-outs)
#include <time.h>

// Menu options for the client
void print_menu(){
  printf("%s\n", "Select an action to perform:");
  printf("%s\n", "----------------------------");
  printf("%s\n", "get [file_name]");
  printf("%s\n", "put [file_name]");
  printf("%s\n", "delete [file_name]");
  printf("%s\n", "ls");
  printf("%s\n\n", "exit");
}

// Check and initialize command line arguments
int init_cmdline(int argc, char * argv[], char ** hostname, char ** port_no) {
  // Check command line arguments
  if (argc != 3) {
      fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
      return -1;
  }

  *hostname = argv[1];
  *port_no = argv[2];
  
  return 0;
}

// Initialize hints struct
int init_hints(struct addrinfo * hints){
  memset((char *) hints, 0, sizeof(struct addrinfo)); // Set the struct to all 0's first
  hints->ai_family = AF_INET; // Use only IPV4
  hints->ai_socktype = SOCK_DGRAM; // UDP
  return 0;
}

// Initialize the socket
int init_socket(struct addrinfo * server_infos, struct addrinfo ** p, int * sock_fd){
  // Loop through linked list from getaddrinfo and create the first socket we can
  for(*p = server_infos; *p != NULL; *p = (*p)->ai_next){

    // Try creating socket
    if(((*sock_fd) = socket(server_infos->ai_family, server_infos->ai_socktype, server_infos->ai_protocol)) < 0){
      perror("ERROR creating socket\n");
      continue; // Try next struct in server_infos
    }
    if(fcntl((*sock_fd), F_SETFL, O_NONBLOCK) == -1){
      perror("ERROR setting non-block socket\n");
      return -1;
    }

    break;
  }

  // Check if we have a socket to a server
  if((*p) == NULL){
    close(*sock_fd);
    freeaddrinfo(server_infos);
    fprintf(stderr, "ERROR creating socket\n");
    return -1;
  }

  return 0;
}

void get_message(uint16_t * bytes_no, char * buf, int bytes){
    // CLEAR BUFFER
    memset(buf, 0, MAX_PACKET_SZ);
    fgets(buf, bytes, stdin);
    *bytes_no = strlen(buf);
    if(bytes_no != 0){
        buf[(*bytes_no)-1] = '\0'; // Make input null-terminating
    }
}

int main(int argc, char **argv) {
  int sock_fd; // Socket file descriptor
  int rv; // Return value used in error printg
  char *hostname = NULL, *port_no = NULL; // Strings of hostname and port number
  char send_buf[MAX_PACKET_SZ]; // Where the payload will be sent from
  char recv_buf[MAX_PACKET_SZ]; // Where the payload will be received
  char user_buf[MAX_USER_BUF]; // Input up to MAX_USER_BUF characters for a command
  struct addrinfo hints, *server_infos, *p = NULL; // Used in socket creation and UDP messaging
  socklen_t from_len = sizeof(struct sockaddr_storage); // Used in socket connection (recvfrom)
  struct sockaddr_storage in_info; // Used in message recieved
  clock_t RTT;

  // Check and initialize command line arguments
  if (init_cmdline(argc, argv, &hostname, &port_no) < 0)
    exit(0);
  
  // Initialize the addrinfo (hints) struct
  if (init_hints(&hints) < 0)
    exit(0);

  // getaddrinfo: get address information of server from hostname and port number
  if((rv = getaddrinfo(hostname, port_no, &hints, &server_infos)) < 0){
    fprintf(stderr, "ERROR looking up given address/port number combo: %s\n", gai_strerror(rv));
    exit(0);
  }

  // Initialize the socket
  if(init_socket(server_infos, &p, &sock_fd) < 0)
    exit(0);

  // Initialize CRC lookup table
  F_CRC_InicializaTabla();

  // Intialize our wrapper struct for our connection data
  connection_info info;
  info.sock_fd = sock_fd;
  info.addr = p->ai_addr;
  info.addrlen = p->ai_addrlen;
  info.in_info = &in_info;
  info.from_len = from_len;
  info.send_buf = send_buf;
  info.recv_buf = recv_buf;

  // Main loop
  while(1){
    print_menu();
    // Input a command
    uint16_t bytes_no;
    get_message(&bytes_no, (char *) user_buf, MAX_USER_BUF);

    // Collect extra packets
    clock_t start = clock();
    while(recvfrom(info.sock_fd, info.recv_buf, MAX_PACKET_SZ, 0, (struct sockaddr *) info.in_info, &info.from_len) == -1){
      if(!(errno == EAGAIN || errno == EWOULDBLOCK)){
        // Error isn't a result of non-blocking, actual error
        perror("ERROR with recvfrom");
        printf("errno %u\n", errno);
        break;
      } else {
        // No error
        // Check if we have timed out
        if(/*start + timeout < clock() &&*/ start + TIMEOUT_VAL < clock()){
          printf("Finished clearing packets...\n");
          break;
        }
      }
    }

    // get [FILE]
    if(!strncmp(user_buf, "get", 3) && strlen(user_buf) > 4){
      int ready_to_transfer = 0;
      while(!ready_to_transfer) {
        // First tell the server we want a file
        if(user_buf + 4 != NULL){
          uint16_t bytes_no = strlen(user_buf+4)+1;
          clock_t timeout = CLOCKS_PER_SEC * BUSY_WAIT_TIMEOUT;
          if((RTT = send_then_confirm_ack(&info, bytes_no, 0x22, 0x22, 0, user_buf+4, timeout)) < 0){
            printf("ERROR asking server to send file\n");
            cleanup(sock_fd, server_infos, 1);
          }
        }

        // Listen for a header/probe from the server
        clock_t timeout = (clock_t) BUSY_WAIT_TIMEOUT * CLOCKS_PER_SEC;
        if((rv = recv_packet(&info, 0x22, timeout)) == 0){
          ready_to_transfer = 1;
        } else if(rv == -2){
          printf("Expected probe packet was a data packet, aborting command...\n");
          continue;
        }
      }

      // Send an ACK to the server acknowledging that
      prep_packet(&info, 0, 0x22, 0, NULL);
      if(PRINT_PACKET_HDRS) print_packet_headers(info.send_buf);
      send_packet(info.sock_fd, info.send_buf, info.addr, info.addrlen);

      uint32_t packets_total = get_packet_no(recv_buf);

      if(packets_total){
        // Extract filename and # of packets from the header received.
        if (get_filename(recv_buf, (char *) user_buf) < 0){
          printf("ERROR in get_filename, skipping rest of the transmission\n");
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
      

    // put [FILE]
    } else if(!strncmp(user_buf, "put", 3) && strlen(user_buf) > 4){
      if(send_file(&info, 0x12, user_buf+4)){
        printf("ERROR in send_file\n");
        cleanup(sock_fd, server_infos, 1);
      }
    } 
    // command
    else if((!strncmp(user_buf, "delete", 6) && strlen(user_buf) > 7) ||
            (!strncmp(user_buf, "ls", 2) && strlen(user_buf) == 2) ){
      
      if(!strncmp(user_buf, "delete", 6) && strlen(user_buf) > 7){
        strncpy(user_buf, "rm ", 4);
        strncpy(user_buf + 3, user_buf+7, MAX_PAYLOAD_SZ);
      }

      int ready_to_transfer = 0;
      while(!ready_to_transfer){
        printf("\nRequesting Command: \"%s\"\n", user_buf);
        request_cmd(&info, user_buf);

        // Listen for a probe from the server to intiate the file transfer back
        clock_t timeout = (clock_t) BUSY_WAIT_TIMEOUT * CLOCKS_PER_SEC;
        if(( rv = recv_packet(&info, 0x32, timeout) ) == 0){
          ready_to_transfer = 1;
        }  else if(rv == -2){
          printf("Expected probe packet was a data packet, aborting command...\n");
          continue;
        }
      }
      
      // Send an ACK to the server acknowledging that we're ready to start the file transfer
      prep_packet(&info, 0, 0x32, 0, NULL);
      if(PRINT_PACKET_HDRS) print_packet_headers(info.send_buf);
      send_packet(info.sock_fd, info.send_buf, info.addr, info.addrlen);

      // Extract filename and # of packets from the header received.
      if (get_filename(recv_buf, (char *) user_buf) < 0){
        printf("ERROR in get_filename, skipping rest of the transmission\n");
        continue;
      }
      uint32_t packets_total = get_packet_no(recv_buf);

      // Start receiving the file
      if ((rv = recv_file(&info, user_buf, packets_total)) < 0){
        if(rv == -2){
          printf("Expected data packet was a probe packet, aborting command...\n");
          continue;
        }
        printf("ERROR in recv_file\n");
        cleanup(sock_fd, server_infos, 1);
      }

      FILE *cmd_out = fopen("cmd_response_temp.txt", "r");
      char c;
      while((c = fgetc(cmd_out)) != EOF){
          printf("%c", c);
      }

      pclose(popen("rm cmd_response_temp.txt", "r"));
    }
    // exit
    else if(!strncmp(user_buf, "exit", 4) && strlen(user_buf) == 4){
      prep_packet(&info, 0, 0x04, 0, NULL);
      send_packet(info.sock_fd, info.send_buf, info.addr, info.addrlen);
      break;
    } else {
      printf("Command not recognized, try again...\n");
    }
  }

  // Clean up
  cleanup(sock_fd, server_infos, 0);
}
