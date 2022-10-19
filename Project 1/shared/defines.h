#ifndef DEFINES_H
#define DEFINES_H

#include <time.h>
// NOTE: tc command will be used to force packet loss

/*
 * Structure of packet we will be creating:
 * 
 * GENERAL PACKET STRUCTURE
 * -----------------------------------------------------------------------------
 * | CRC32 [4] | FLAGS [1] | PACKET_NO [4] | BYTES_NO [2] | PAYLOAD ...        |
 * -----------------------------------------------------------------------------
 * 
 * PAYLOAD PACKET
 * -----------------------------------------------------------------------------
 * | CRC32 [4] |    0x00   | PACKET_NO [4] | BYTES_NO [2] | PAYLOAD ...        |
 * -----------------------------------------------------------------------------
 * 
 * ACK PACKET
 * -----------------------------------------------------------------------------
 * | CRC32 [4] |    0x01   | PACKET_NO [4] |  ...                              |
 * -----------------------------------------------------------------------------
 * ACK_NO : sizeof(PACKET_NO) bytes long, contains packet number being ACKed
 *                    
 * PROBE PACKET        
 * --------------------------------------------------------------------------------------------------------------------
 * | CRC32 [4] |    0xX2   | PACKET_NO [4] | BYTES_NO [2] | PACK_TOTAL[4] (not 0x30) FILENAME/COMMAND (yes 0x30) ...  |
 * --------------------------------------------------------------------------------------------------------------------
 * PACK_TOTAL : contains the number of packets that will be sent to the receiver (same size as PACKET_NO)
 * 0xX0 : The type of operation we will be performing
 *      - 0x10 : "put" operation
 *      - 0x20 : "get" operation
 *      - 0x30 : some command operation
 * 
 * CRC32 : 4 bytes long (8*4 = 32 bits)
 * FLAGS : 1 byte, 0x01 means the packet is an ACK, 0x00 means it is not, 0x02 means probe packet
 * PACKET_NO : 4 bytes long allows for more than enough room for 10MB files
 * BYTES_NO : 2 bytes long since we are storing the # of byte that are useful in the payload (more than 255)
 * PAYLOAD: MAX_PAYLOAD - IPV4_MAX_HDR_SZ - UDP_MAX_HDR_SZ - PACKET_NO_SIZE - CRC_SIZE
 */

// Calculate payload size in bytes
#define MAX_USER_BUF 100
#define MAX_PACKET 1452 // Chosen from wikipedia https://en.wikipedia.org/wiki/Maximum_transmission_unit
#define IPV4_MAX_HDR_SZ 60 // Subtract from total size
#define UDP_MAX_HDR_SZ 8 // Subtract form total size
#define MAX_PACKET_SZ MAX_PACKET - IPV4_MAX_HDR_SZ - UDP_MAX_HDR_SZ

// Custom header sizes in bytes
#define CRC_SIZE 4 // 32 bits = 4 bytes needed for CRC
#define FLAGS_SIZE 1 // 8 bits used for various flags
                     //the first bit indicates whether the packet is an ACK (1) or not (0)
#define PACKET_NO_SIZE 4 // 4 bytes to allow for more than enough for 10MB file sizes
                         // (including our headers defined above)
#define BYTES_NO_SIZE 2 // 2 bytes allows for counting more than 255 bytes in the paylaod
#define MAX_PAYLOAD_SZ MAX_PACKET_SZ - PACKET_NO_SIZE - CRC_SIZE - BYTES_NO_SIZE - FLAGS_SIZE

// Used for array indexing
#define CRC_OFFSET 0
#define FLAGS_OFFSET CRC_SIZE
#define PACKET_NO_OFFSET CRC_SIZE + FLAGS_SIZE
#define BYTES_NO_OFFSET  CRC_SIZE + FLAGS_SIZE + PACKET_NO_SIZE
#define PAYLOAD_OFFSET CRC_SIZE + FLAGS_SIZE + PACKET_NO_SIZE + BYTES_NO_SIZE
#define FILENAME_OFFSET PAYLOAD_OFFSET + PACKET_NO_SIZE

// Used for sliding window
#define WINDOW_SZ 5  // How many packets are in the sliding window
                     // MUST BE LESS THAN (2^BYTES_NO_SIZE)/2
#define RTT_MULT 10.0 // After getting a RTT time, multiply by RTT_MULT for a suitable time-out for a packet
#define TIMEOUT_VAL (clock_t) (0.1 * CLOCKS_PER_SEC) // In MS

// Used for print statements and debugging
#define PRINT_PACKET_HDRS 0
#define PRINT_PACKET_IP_PORT 0
#define PRINT_PACKET_NUMBER 1
#define BUSY_WAIT_TIMEOUT 5
#define OVERRIDE_RTT 0

#endif