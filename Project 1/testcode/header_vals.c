// Print header values :)
#include <stdio.h>
#include "../shared/defines.h"

int main(){
    printf("MAX_USER_BUF.....%u\n", MAX_USER_BUF);
    printf("MAX_PACKET.......%u\n", MAX_PACKET);
    printf("IPV4_MAX_HDR_SZ..%u\n", IPV4_MAX_HDR_SZ);
    printf("UDP_MAX_HDR_SZ...%u\n", UDP_MAX_HDR_SZ);
    printf("MAX_PACKET_SZ....%u\n", MAX_PACKET_SZ);
    printf("\n");
    printf("CRC_SIZE.........%u\n", CRC_SIZE);
    printf("FLAGS_SIZE.......%u\n", FLAGS_SIZE);
    printf("PACKET_NO_SIZE...%u\n", PACKET_NO_SIZE);
    printf("BYTES_NO_SIZE....%u\n", BYTES_NO_SIZE);
    printf("MAX_PAYLOAD_SZ...%u\n", MAX_PAYLOAD_SZ);
    printf("\n");
    printf("CRC_OFFSET.......%u\n", CRC_OFFSET);
    printf("FLAGS_OFFSET.....%u\n", FLAGS_OFFSET);
    printf("PACKET_NO_OFFSET.%u\n", PACKET_NO_OFFSET);
    printf("BYTES_NO_OFFSET..%u\n", BYTES_NO_OFFSET);
    printf("PAYLOAD_OFFSET...%u\n", PAYLOAD_OFFSET);
    printf("FILENAME_OFFSET...%u\n", FILENAME_OFFSET);
    printf("\n");
    printf("WINDOW_SZ........%u\n", WINDOW_SZ);
    printf("\n");
    printf("PRINT_PACKET_HDRS....%u\n", PRINT_PACKET_HDRS);
    printf("PRINT_PACKET_IP_PORT.%u\n", PRINT_PACKET_IP_PORT);
    printf("PRINT_PACKET_NUMBER..%u\n", PRINT_PACKET_NUMBER);
    printf("\n");
    printf("TIMEOUT_VAL..........%lu\n", TIMEOUT_VAL);
    printf("\n");
    return 0;
}