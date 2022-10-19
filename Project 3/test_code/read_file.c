#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include "../server.h"

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
    // Return to start of file
    if(fseek(file, 0, SEEK_SET) < 0){
        printf("get_file_size: ERROR with fseek\n");
        return -1;
    }
    return file_sz;
}

// Given a host_ip and a host_name, save the result in resolved.txt
int save_ip(char *host_name, char *host_ip){
    // Create cmd string
    char *cmd = malloc(MAX_URI);
    strcpy(cmd, "echo \"");
    strcpy(cmd + 6, host_name);
    strcpy(cmd + strlen(host_name) + 6, " ");
    strcpy(cmd + strlen(host_name) + 7, host_ip);
    strcpy(cmd +  strlen(host_name) + strlen(host_ip) + 7, "\" >> resolved.txt");

    printf("\"%s\"\n", cmd);

    FILE *cmd_response;

    if((cmd_response = popen(cmd, "r")) == NULL){
        fprintf(stderr, "get_md5: ERROR calling \"popen\" with command \"%s\"\n", cmd);
        return -1;
    }

    if(pclose(cmd_response)){
        fprintf(stderr, "get_md5: ERROR with pclose\n");
    }
    return 0;
}

// Given a host_name, save the host_ip to host_ip of size MAX_URI
int get_ip(char *host_name, char *host_ip){
    FILE *file;
    char buf[MAX_URI];
    memset(buf, 0, MAX_URI);

    if((file = fopen("resolved.txt", "rb")) == NULL){
        fprintf(stderr, "File `resolved.txt` cannot be opened\n");
        return -1;
    }

    uint32_t bytes_left = get_file_size(file);
    uint32_t bytes_read = 0;
    uintptr_t offset = 0; // Used when we want to read at a position within the buffer

    while(bytes_left > 0){
        char *start = buf; // Pointer to next starting location for each printing
        char *end = NULL;  // Pointer to next starting location for each printing

        // Read data
        uint32_t bytes = bytes_left < MAX_URI - 1 ? bytes_left : MAX_URI - 1 ;
        if((bytes_read = (uint32_t) fread(buf + offset, 1, bytes - offset, file)) <= 0){
            fprintf(stderr, "Error with fread\n");
            fclose(file);
            return -1;
        }
        
        // Print each line
        char name1[MAX_URI];  // First column 
        char name2[MAX_URI];  // Second column
        while((end = strchr(start, '\n')) != NULL){

            if(start == end){
                continue;
            }

            char *space = strchr(start, ' ');
            if(space == NULL){
                fprintf(stderr, "Error with finding space in `resolved.txt`\n");
                fclose(file);
                return -1;
            }
            memset(name1, 0, MAX_URI); memset(name2, 0, MAX_URI);
            strncpy(name1, start, space - start); // 123 456\n
            strncpy(name2, space+1, end - (space + 1));

            printf("\"%s\" \"%s\"\n", name1, name2);

            // Check if we have a hit
            if(strcmp(name1, host_name) == 0){
                strncpy(host_ip, name2, MAX_URI - 1);
                fclose(file);
                return 0;
            }

            // Change start and end
            start = end + 1; // +1 so that we don't start on the newline
            end = NULL;
        }
        // No more newlines

        // Check if buffer is big enough
        if(start == buf){
            fprintf(stderr, "Error extracting from `resolved.txt`, buffer not big enough\n");
            fclose(file);
            return -1;
        }

        // update values before re-reading
        bytes_left -= (uint32_t) (start - buf); // +1 so that we don't start on a newline next read
        offset = strlen(start);

        // Copy remaining data into buffer at the start
        memcpy(buf, start, strlen(start));
        memset(buf + offset, 0, MAX_URI - offset);
    }

    fclose(file);
    return -1;
}

int main(int argc, char * argv[]){
    char ret[MAX_URI];
    memset(ret, 0, MAX_URI);
    if(get_ip("hello_bro", ret) < 0){
        printf("Couldn't find \"hello_bro\"\n");
        save_ip("hello_bro", "omg_bro");
    } else {
        printf("Returned \"%s\" for \"hello_bro\"\n", ret);
    }

    return 0;
}