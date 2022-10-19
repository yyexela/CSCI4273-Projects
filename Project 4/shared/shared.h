#ifndef SHARED_H
#define SHARED_H

#define MAX_LINE            128     // We know that each line in dfc.conf is less than 128 bytes, also limits user input
#define MAX_PORT            8       // We know port numbers can be stored in less than 8 chars (XXXXX)
#define MAX_IP              16      // We know IPV4 addresses can be stored in less than 16 chars (XXX.XXX.XXX.XXX)
#define MAX_DIR             8       // We know that each server cache directory is less than 8 bytes (DSX)
#define LISTENQ             1024    // Max # of connections on server ports (used in listen())
#define TIMEOUT             1       // How long we try connecting to a server for (in seconds)
#define MAX_WRITE_SZ        4096    // Max number of bytes send per `write`
#define MAX_READ_SZ         4096    // Max number of bytes send per `read`

#define PRINTS              0       // 0/1 to disable/enable ALL non-error print statements
#define PRINT_UPDATE_SERVER 0
#define PRINT_USER_INPUT    0
#define PRINT_SERVER_CONN   1
#define PRINT_CONN_ERR      0
#define PRINT_FILE_SIZE     0
#define PRINT_MD5           1
#define PRINT_PART_SZ       1
#define PRINT_METADATA      1
#define PRINT_DOWNLOAD_INFO 0
#define PRINT_SEND_INFO     0
#define PRINT_RECV          0
#define PRINT_OFFSET        1
#define PRINT_GET_FILES     1

// Send the string pointed to bt char *str (must be null terminated)
// Returns 1 on "caught errors" from clean_write
int send_string(char *str, int server_fd);

// Send the file, pass in how much of the file (filesize) to send
int send_file(FILE * file, int server_fd, size_t filesize);

// Write to socket catching errors when the socket is closed (returns 0),
// other errors (returns -1), and the bytes written (returns bytes)
// caught errors (socket close, EPIPE, etc..) (returns 1)
int clean_write(int fd, void *buf, size_t n);

// Given a valid open file, return its size or -1 on error
uint32_t get_file_size(FILE * file);

#endif
