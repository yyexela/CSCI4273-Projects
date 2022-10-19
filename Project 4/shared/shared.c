#include <stdio.h>      // For print statements
#include <unistd.h>     // For File IO
#include <string.h>     // For string manipulation
#include <signal.h>     // For signal handling
#include <stdlib.h>     // For exit() function
#include <sys/types.h>  // For TCP/Networking
//#include <fcntl.h>      // - Used for non-blocking sockets
#include <sys/socket.h> 
#include <netdb.h>
#include <time.h>       // Used for timing out when connecting to servers   
#include <errno.h>      // Used for catching errors
#include "shared.h"

int send_string(char *str, int fd){
    size_t bytes_left, bytes_written;
    int rv;
    bytes_left = (size_t) strlen(str); // bytes left in the str to send
    bytes_written = 0; // bytes written from the str
    while(bytes_left > 0){
        size_t bytes_no = bytes_left > MAX_WRITE_SZ ? MAX_WRITE_SZ : bytes_left; // bytes to write this call
        rv = clean_write(fd, str + (bytes_written), bytes_no);
        if(rv == 0){
            return 1;
        } else if (rv < 0){
            fprintf(stderr, "send_string: ERROR with clean_write\n");
            return rv;
        }
        bytes_left -= bytes_no;
        bytes_written += bytes_no;
    }
    return 0;
}

int send_file(FILE * file, int fd, size_t filesize){
    size_t bytes_left, bytes_written;
    char file_buf[MAX_WRITE_SZ];
    bytes_written = (size_t) 0; // bytes written from the file
    bytes_left = filesize;
    int rv;

    if(PRINTS & PRINT_SEND_INFO) printf("bytes_left %ld\n", bytes_left);

    // For the rest of the data in the file (or the entire file for GET requests)
    // read the file in chunks in to a file buffer and then write that file buffer
    // into the socket repeatedly until EoF
    while(bytes_left > (size_t)0 ){
        size_t bytes_no = bytes_left > MAX_WRITE_SZ ? MAX_WRITE_SZ : bytes_left; // bytes to write this call
        if(PRINTS & PRINT_SEND_INFO) printf("Sending %ld\n", bytes_no);
        if(fread((void *) file_buf, (size_t) 1, bytes_no, file) == 0){
            fprintf(stderr, "send_part: ERROR with fread (filesize %ld)\n", 
                filesize);
            return -1;
        }
        rv = clean_write(fd, file_buf, bytes_no);
        if(rv == 0){
            return 1;
        } else if (rv < 0){
            fprintf(stderr, "send_part: ERROR with clean_write\n");
            return rv;
        }
        bytes_left -= bytes_no;
        bytes_written += bytes_no;
    }
    return 0;
}

int clean_write(int fd, void *buf, size_t n){
    ssize_t bytes = 0;
    if((bytes = write(fd, buf, n)) <= 0){
        if(errno == EAGAIN){
            fprintf(stderr, "clean_write: ERROR with write, caught EAGAIN\n");
    	} else if(errno == EPIPE){
            fprintf(stderr, "clean_write: ERROR with write, caught EPIPE\n");
            return 0;
        } else if(errno == 104){
            fprintf(stderr, "clean_write: ERROR with write, caught \"Connnection reset by peer\"\n");
            return 0;
        } else {
            fprintf(stderr, "clean_write: ERROR with write, errno of %d\n", errno);
            perror("");
            return -1;
        }
    }
    return bytes;
}

uint32_t get_file_size(FILE * file){
    uint32_t file_sz;
    // Calculate file size in bytes
    if(fseek(file, 0, SEEK_END) < 0){
        printf("get_file_size: ERROR with fseek\n");
        return -1;
    }
    if((file_sz = ftell(file)) < 0){
        printf("get_file_size: ERROR ftell\n");
        return -1;
    }
    if(PRINTS & PRINT_FILE_SIZE) printf("File size : %u bytes\n", file_sz);
    // Return to start of file
    if(fseek(file, 0, SEEK_SET) < 0){
        printf("get_file_size: ERROR with fseek\n");
        return -1;
    }
    return file_sz;
}