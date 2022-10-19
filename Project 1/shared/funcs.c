#include "funcs.h"
#include "defines.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include "crc.h"

// error - wrapper for perror
void error(char *msg) {
    perror(msg);
    exit(0);
}

// Given a packet defined in 'defines.h', return the included crc value
uint32_t get_crc(char * packet){
    return ntohl(*((uint32_t *) (packet + CRC_OFFSET)));
}

// Given a packet defined in 'defines.h', return the included packet_no value
uint32_t get_packet_no(char * packet){
    return ntohl(*((uint32_t *) (packet + PACKET_NO_OFFSET)));
}

// Given a packet defined in 'defines.h', return the included packets_total value (probe packets only)
uint32_t get_packets_total(char * packet){
    return ntohl(*((uint32_t *) (packet + PAYLOAD_OFFSET)));
}

// Given a packet defined in 'defines.h', return the included bytes_no value
uint16_t get_bytes_no(char * packet){
    return ntohs(*((uint16_t *) (packet + BYTES_NO_OFFSET)));
}

// Given a packet defined in 'defines.h', return the included flags value
uint8_t get_flags(char * packet){
    return *((uint8_t *) (packet + FLAGS_OFFSET));
}

// Given a packet defined in 'defines.h', set the crc value
void set_crc(char * packet, uint32_t crc){
    *((uint32_t *) (packet + CRC_OFFSET)) = htonl(crc);
}

// Given a packet defined in 'defines.h', set the packet_no value
void set_packet_no(char * packet, uint32_t packet_no){
    *((uint32_t *) (packet + PACKET_NO_OFFSET)) = htonl(packet_no);
}

// Given a packet defined in 'defines.h', set the packet_total value (probe packets only)
void set_packets_total(char * packet, uint32_t packets_total){
    *((uint32_t *) (packet + PAYLOAD_OFFSET)) = htonl(packets_total);
}

// Given a packet defined in 'defines.h', set the bytes_no value
void set_bytes_no(char * packet, uint16_t bytes_no){
    *((uint16_t *) (packet + BYTES_NO_OFFSET)) = htons(bytes_no);
}

// Given a packet defined in 'defines.h', set the flags value
void set_flags(char * packet, uint8_t flags){
    *((uint8_t *) (packet + FLAGS_OFFSET)) = flags;
}

// Given a packet defined in 'defines.h'
void clear_packet(char * packet){
    memset(packet, 0, MAX_PACKET_SZ);
}

// Send a UDP packet, not guaranteeing anything
void send_packet(int sock_fd, char * buf, struct sockaddr * addr, socklen_t addr_len){
    if(sendto(sock_fd, buf, MAX_PACKET_SZ, 0, addr, addr_len) < 0){
        close(sock_fd);
        error("ERROR sending message\n");
    }
}

void print_packet_headers(char * buf){
    // If we're printing we don't care about efficiency lmoa

    // Extract packet info
    uint16_t bytes_no = get_bytes_no((char *) buf); // Get the number of bytes
    uint32_t packet_no = get_packet_no((char *) buf); // Get the packet number
    uint32_t crc_pack = get_crc((char *) buf); // Get the CRC value in the packet
    uint8_t flags = get_flags((char *) buf); // Get the flags

    printf("CRC:\t\t0x%x\nPacket Number:\t%hu\nBytes Number:\t%hu\nFlags:\t\t0x%hhx\n",
      crc_pack, packet_no, bytes_no, flags
      );
}

void print_ip_and_port(struct sockaddr_storage * in_info){
    char s[INET_ADDRSTRLEN]; // Used in message recieved to print IP address
    printf("Packet from\t%s:%hu\n",
      inet_ntop(in_info->ss_family, &((struct sockaddr_in *) in_info)->sin_addr, s, sizeof(s)), 
      ntohs(((struct sockaddr_in *) in_info)->sin_port)
      );
}

int check_packet_crc(char * buf){
    uint32_t crc_pack = get_crc(buf);
    uint16_t bytes_no = get_bytes_no(buf);

    // Recalculate CRC
    // NOTE: A checksum is calculated for the bytes we are sending but NOT the CRC field
    uint32_t crc_calc = F_CRC_CalculaCheckSum((void *) buf + CRC_SIZE, bytes_no + FLAGS_SIZE + PACKET_NO_SIZE + BYTES_NO_SIZE);
    //printf("Comparing crc_pack\n(0x%x)\nand crc_calc\n(0x%x)\n\n", crc_pack, crc_calc);

    // Compare CRC
    if(crc_pack != crc_calc){
      // MESSAGE CORRUPT
        printf("CRC mismatch (0x%x)\n", crc_calc);
        return -1;
    }

    return 0;
}

int send_ack(connection_info *info, uint32_t packet_no){

  prep_packet(info, 0, 0x01, packet_no, NULL);
  printf("Sending ack for packet #%u\n", packet_no);
  if(PRINT_PACKET_HDRS) print_packet_headers(info->send_buf);
  send_packet(info->sock_fd, info->send_buf, info->addr, info->addrlen);

  return 0;
}

// Get filename from probe packet
int get_filename(char * recv_buf, char * filename){
  uint16_t bytes_no = get_bytes_no(recv_buf);
  strncpy(filename, recv_buf+PAYLOAD_OFFSET, bytes_no);
  return 0;
}

// Cleanly closes a socketed and exits the program
void cleanup(int sock_fd, struct addrinfo *server_infos, int exitcode){
  close(sock_fd);
  freeaddrinfo(server_infos);
  exit(exitcode);
}

// Given the necessary information, this creates a complete packet with a filled in
// header and payload.
int prep_packet(connection_info *info, uint16_t bytes_no, uint8_t flag, uint32_t packet_no, char *payload){
  clear_packet(info->send_buf);
  set_packet_no(info->send_buf, packet_no);
  set_bytes_no(info->send_buf, bytes_no); 
  set_flags(info->send_buf, flag);
  if(payload != NULL){
    memcpy(info->send_buf + PAYLOAD_OFFSET, payload, bytes_no);
  }
  // CRC checksum
  // NOTE: A checksum is calculated for the bytes we are sending but NOT the CRC field
  uint32_t crc = F_CRC_CalculaCheckSum((void *) info->send_buf + CRC_SIZE, bytes_no + FLAGS_SIZE + PACKET_NO_SIZE + BYTES_NO_SIZE);
  set_crc((char *) info->send_buf, crc);

  return 0;
}

// Receive a packet, checking for timeout, crc, and flags
int recv_packet(connection_info *info, uint8_t expected_flag, clock_t timeout){
  clear_packet(info->recv_buf);

  // Start clock
  clock_t start = clock();

  // Listen for a message from the server and populate in_info with sender info
  while(recvfrom(info->sock_fd, info->recv_buf, MAX_PACKET_SZ, 0, (struct sockaddr *) info->in_info, &info->from_len) == -1){
    if(!(errno == EAGAIN || errno == EWOULDBLOCK)){
      // Error isn't a result of non-blocking, actual error
      perror("ERROR with recvfrom");
      printf("errno %u\n", errno);
      return -1;
    } else {
      // No error
      // Check if we have timed out
      if(/*start + timeout < clock() &&*/ start + TIMEOUT_VAL < clock() && timeout != -1){
        printf("Timed out...\n");
        return -1;
      }
    }
  }

  if(check_packet_crc(info->recv_buf) < 0){
    printf("Received packet CRC incorrect...\n");
    return -1;
  }

  // Print the message 
  if(PRINT_PACKET_IP_PORT) print_ip_and_port(info->in_info);
  if(PRINT_PACKET_HDRS) print_packet_headers(info->recv_buf);
  if(PRINT_PACKET_IP_PORT | PRINT_PACKET_HDRS) printf("\n");

  // Check flags 
  uint8_t received_flag = get_flags(info->recv_buf);
  if(received_flag != expected_flag){
    printf("FLAGS aren't 0x%hhx, they are 0x%hhx\n", expected_flag, received_flag);
    return -2;
  }

  return 0;
}

// Prepares and sends a payload, waits for an ACK and resends and rewaits if anything doesn't work.
clock_t send_then_confirm_ack(connection_info *info, uint16_t bytes_no, uint8_t send_flag, uint8_t ack_flag, uint32_t packet_no, char *payload, clock_t timeout){
  int confirmed = 0;

  while(!confirmed){
    prep_packet(info, bytes_no, send_flag, packet_no, payload);
    if(PRINT_PACKET_HDRS) print_packet_headers(info->send_buf);
    send_packet(info->sock_fd, info->send_buf, info->addr, info->addrlen);
    if(recv_packet(info, ack_flag, timeout) == 0){
      confirmed = 1;
    }
  }

  return TIMEOUT_VAL;
}

int recv_file(connection_info *info, char *filename, uint32_t packets_total){
  int ret;
  FILE * file = fopen(filename, "wb+");

  if(file == NULL){
    printf("ERROR creating file \"%s\"\n", filename);
    return -1;
  }

  for(uint32_t i = 0; i < packets_total; i++){
    while((ret = recv_packet(info, 0x0, -1)) != 0){
      if(ret == -2) return -2;
    }

    printf("Received packet while i = %u\n", i);
    // Make sure it's the packet type we want
    uint32_t packet_no = get_packet_no(info->recv_buf);
    if (packet_no != i){
      // printf("ERROR out of order packet, wanted %u got %u\n", i, packet_no);
      if(packet_no < i){
        // printf("Out of order packet w as already received, re-sending ACK\n");
        send_ack(info, packet_no);
        i--;
        continue;
      }
    }

    // Everything good
    // Write into buffer until we wrote bytes_no of things
    uint16_t bytes_no = get_bytes_no(info->recv_buf);
    if(fwrite(info->recv_buf + PAYLOAD_OFFSET, 1, bytes_no, file) != bytes_no){
        printf("ERROR with fwrite\n");
        fclose(file);
        return -1;
    }
    // Send an ACK
    send_ack(info, i);
  }

  fclose(file);
  return 0;
}

int send_file(connection_info *info, uint8_t flag, char *filename){
  uint32_t packets_total = 0;
  uint32_t file_sz = 0;
  uint16_t bytes_no = 0;
  clock_t RTT;

  // Open file to send
  FILE *file = fopen(filename, "rb");
  if(file == NULL){
    printf("FILE \"%s\" NOT FOUND\n", filename);
    packets_total = 0;
  } else {
      // Calculate file size in bytes
    file_sz = 0;
    if(fseek(file, 0, SEEK_END) < 0){
      printf("ERROR GETTING FILE SIZE (fseek)\n");
      return -1;
    }
    if((file_sz = ftell(file)) < 0){
      printf("ERROR GETTING FILE SIZE (ftell)\n");
      return -1;
    }
    printf("File size : %u bytes\n", file_sz);

    // Calculate total number of packets we'll send
    packets_total = (uint32_t) ((0.0 + file_sz) / (0.0 + MAX_PAYLOAD_SZ));

    // Round up
    if (file_sz > packets_total * ((uint32_t) MAX_PAYLOAD_SZ)) packets_total++;

    bytes_no = strlen(filename)+1;
  }

  printf("Need %u packets\n\n", packets_total);

  clock_t timeout = CLOCKS_PER_SEC * BUSY_WAIT_TIMEOUT;
  if((RTT = send_then_confirm_ack(info, bytes_no, flag, flag, packets_total, filename, timeout)) < 0){
    printf("ERROR in send_then_confirm_ack\n");
    return RTT;
  }
  printf("RTT in s:\t%f\n\n", (double) ((RTT + 0.0)/CLOCKS_PER_SEC));

  // Keep track of how many bytes of the file are left
  uint32_t remaining_bytes = file_sz;

  // Server now knows what we're doing and should now be ready to receive our file
  // We will be sending one packet at a time
  for(uint32_t i = 0; i < packets_total; i++){
    uint16_t bytes_no = remaining_bytes > MAX_PAYLOAD_SZ ? MAX_PAYLOAD_SZ : remaining_bytes;   
    char file_seg[bytes_no];

    // Seek to current packet position
    if(fseek(file, i * ((uint32_t) MAX_PAYLOAD_SZ), SEEK_SET) < 0){
      printf("ERROR with fseek :(\n");
      fclose(file);
      return -1;
    }
    if(fread(file_seg, 1, bytes_no, file) != bytes_no){
      printf("ERROR with fread\n");
      i--;
      continue;
    }
    
    send_then_confirm_ack(info, bytes_no, 0x00, 0x01, i, file_seg, RTT);

    uint32_t packet_no = get_packet_no(info->recv_buf);
    if(PRINT_PACKET_NUMBER) printf("Received ACK for packet #%u\n", packet_no);

    clock_t start = clock();
    while(packet_no != i){
      printf("Received ACK for incorrect packet #, waitng for correct ACK\n");
      if(recv_packet(info, 0x01, RTT) == 0){
        packet_no = get_packet_no(info->recv_buf);
        if(PRINT_PACKET_NUMBER) printf("Received ACK for packet #%u\n", packet_no);
      }
      if(start + TIMEOUT_VAL < clock()){
        break;
      }
    }

    if(packet_no != i){
      printf("Timed out waiting for correct ACK, resending packet\n");
      i--;
      continue;
    }

    remaining_bytes -= bytes_no;
  }

  if(file != NULL) fclose(file);

  return 0;
}

int request_cmd(connection_info *info, char *command){
  uint16_t bytes_no = strlen(command) + 1;
  clock_t timeout = (clock_t) BUSY_WAIT_TIMEOUT * CLOCKS_PER_SEC;
  if(send_then_confirm_ack(info, bytes_no, 0x32, 0x32, 0, command, timeout)) return -1;
  return 0;
}

