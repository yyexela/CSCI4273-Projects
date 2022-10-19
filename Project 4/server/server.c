/* 
 * TODO:
 * 
 * DOING:
 * 
 * DONE:
 * 
 */

#include <stdio.h>      // For print statements
#include <unistd.h>     // For File IO
#include <string.h>     // For string manipulation
#include <stdlib.h>     // For `system` system call (run bash scripts)
#include <sys/types.h>  // For TCP/Networking
//#include <fcntl.h>      // - Used for non-blocking sockets
#include <sys/socket.h> 
#include <netdb.h>      
#include <pthread.h>    // Multi-threading
#include <sys/stat.h>   // For `mkdir` C function
#include <errno.h>      // For errno checking
#include <stdint.h>     // For size_t
#include "server.h"     // For defines and structs
#include "../shared/shared.h"     // For defines and structs

int main(int argc, char *argv[]){
    char * port = argv[2];
    char * dir = argv[1];
    int sock_fd, conn_fdp;
    unsigned int clientlen = sizeof(struct sockaddr_in);
    struct sockaddr_in client_addr;
    pthread_t tid; 

    // Check command line arguments are correct
    if(init_cmdline(argc, argv) < 0){
        fprintf(stderr, "main: Commandline incorrect\n");
        exit(0);
    }

    // Make sure all DFS folders exist
    if(system("../bash_scripts/DFS_folders.sh") != 0){
        fprintf(stderr, "main: Error with DFS_folders.sh\n");
        exit(0);
    }

    // Bind to a socket
    if((sock_fd = open_listenfd(atoi(port))) == -1){
        fprintf(stderr, "main: Error in creating listen socket\n");
        exit(0);
    }

    // Main loop creating threads to service requests
    while (1){
        // Accept a connection, blocking if there are none
        conn_fdp = accept(sock_fd, (struct sockaddr*)&client_addr, &clientlen);

        // Create command line arguments to pass into the threads
        Info *info = (Info *) malloc(sizeof(Info));
        memset(info, 0, sizeof(Info));
        strcpy(info->dir, dir);
        strcpy(info->port, port);
        info->conn_fd = conn_fdp;

        //fcntl(*conn_fdp, F_SETFL, O_NONBLOCK); // Set socket to non-blocking

        // Create thread
        pthread_create(&tid, NULL, thread_main, info);
    }
}

/* thread routine */
void *thread_main(void *vargp){
    char buf[MAX_READ_SZ];
    memset(buf, 0, MAX_READ_SZ);
    int bytes_read;
    Info *info = (Info *) vargp;
    int conn_fd = info->conn_fd;

    // Don't require the parent thread to call join to collect resources
    pthread_detach(pthread_self()); 

    // Initial read, check why the tcp connection was created
    bytes_read = read(conn_fd, buf, MAX_READ_SZ - 1); 
    if(bytes_read == -1){
        fprintf(stderr, "thread_main: Error calling read\n");
        free(info);
        close(conn_fd);
        return NULL;
    }
    if(PRINTS & PRINT_RECV) printf("Got \"%s\"\n", buf);

    // Check type
    if(strncmp(buf, "put", 3) == 0){
        if(recv_file(conn_fd, buf, info->dir, bytes_read) < 0){
            fprintf(stderr, "thread_main: Error in calling receiving file\n");
            free(info);
            close(conn_fd);
            return NULL;
        }
    } else if(strncmp(buf, "list", 4) == 0){
        if(return_query(conn_fd, info->dir) < 0){
            fprintf(stderr, "thread_main: Error in calling return_query\n");
            free(info);
            close(conn_fd);
            return NULL;
        }
    } else if(strncmp(buf, "get", 3) == 0){
        if(run_put(conn_fd, buf, info->dir, bytes_read)){
            fprintf(stderr, "thread_main: Error in calling run_put\n");
            free(info);
            close(conn_fd);
            return NULL;
        }
    }

    free(info);
    close(conn_fd);
    return NULL;
}

int return_query(int conn_fd, char *dir){
    int rv;

    // Construct bash command
    char cmd[MAX_WRITE_SZ];
    memset(cmd, 0, MAX_WRITE_SZ);
    sprintf(cmd, "../bash_scripts/get_files.sh %s", dir);
    if(PRINTS & PRINT_GET_FILES) printf("RUNNING %s\n", cmd);

    // Save results to bash command in `buf`
    char buf[MAX_WRITE_SZ];
    memset(buf, 0, MAX_WRITE_SZ);
    FILE *cmd_response;

    // Run command `get_files.sh` to see which parts of which files we have
    if((cmd_response = popen(cmd, "r")) == NULL){
        fprintf(stderr, "return_query: ERROR calling \"popen\" with command \"%s\"\n", cmd);
        return -1;
    }

    // Read results of bash command from pipe
    size_t bytes_read = 0, pos = 0;
    while((bytes_read = fread(buf + pos, 1, MAX_WRITE_SZ, cmd_response)) > 0){
        pos += bytes_read;
    }

    // Close the pipe
    if(pclose(cmd_response)){
        fprintf(stderr, "return_query: ERROR with pclose\n");
        return -1;
    }

    if(PRINTS & PRINT_GET_FILES) printf("Response:\n---------\n%s\n---------\n", buf);

    // write the data
    rv = send_string(buf, conn_fd);
    if(rv == 1){
        fprintf(stderr, "return_query: Caught error with send_string, returning 0\n");
        return 0;
    } else if(rv < 0){
        fprintf(stderr, "return_query: Error with send_string\n");
        return rv;
    }

    return 0;
}

int recv_file(int conn_fd, char *buf, char *dir, int bytes_read){
    char filename[MAX_LINE];
    memset(filename, 0, MAX_LINE);
    int filesize;
    int part_no;
    int metabytes; // How many bytes the metadata in buf take up (all data not including file data)
    FILE * file;

    // Extract information of file we're going to save
    // <FILENAME> <FILESIZE> <FILE PART NUMBER>
    if(get_put_data(buf, filename, &filesize, &part_no, &metabytes) == NULL){
        fprintf(stderr, "recv_file: Error extracting metadata\n");
        return -1;
    }

    // Make directory path (directory name is `filename`, store parts here
    // with filename equal to part number)
    char dirpath[MAX_WRITE_SZ];
    memset(dirpath, 0, MAX_WRITE_SZ);
    sprintf(dirpath, "%s/%s", dir, filename);

    // Create the directory
    if(mkdir(dirpath, S_IRWXU) != 0 && errno != EEXIST){
        fprintf(stderr, "recv_file: Error in creating directory for \"%s\"\n", dirpath);
        return -1;
    }

    // Construct file path where we'll store the file
    char filepath[MAX_WRITE_SZ];
    memset(filepath, 0, MAX_WRITE_SZ);
    sprintf(filepath, "%s/%s/%d", dir, filename, part_no);

    // Open file to write to
    file = fopen(filepath, "wb+");
    if(file == NULL){
        fprintf(stderr, "recv_file: Error in opening file for \"%s\"\n", filepath);
        return -1;
    }

    // Write the file data we read during the first call to read into the file
    // then keep reading from the socket if there's more data to put in the file
    if(write_put_data(conn_fd, file, buf, bytes_read, metabytes, filesize) < 0){
        fprintf(stderr, "recv_file: Error with write_data\n");
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

// Given a line from dfc.conf, entries in the server struct
char * get_put_data(char *buf, char *filename, int *filesize, int *part_no, int * metabytes){
    char * slash, * space1, * space2, * space3, * space4;
    char line[MAX_LINE];

    // Get character positions
    if((space1 = strchr(buf, ' ')) == NULL){
        fprintf(stderr, "get_put_data: Failed to parse metadata for space1\n");
        return NULL;
    }
    if((space2 = strchr(space1 + 1, ' ')) == NULL){
        fprintf(stderr, "get_put_data: Failed to parse metadata for space2\n");
        return NULL;
    }
    if((space3 = strchr(space2 + 1, ' ')) == NULL){
        fprintf(stderr, "get_put_data: Failed to parse metadata for space3\n");
        return NULL;
    }
    if((space4 = strchr(space3 + 1, ' ')) == NULL){
        fprintf(stderr, "get_put_data: Failed to parse metadata for space4\n");
        return NULL;
    }

    // Extract values
    *metabytes = space4 - buf + 1; // +1 for null-terminating character

    memset(line, 0, MAX_LINE);
    strncpy(line, space2 + 1, space3 - space2 - 1);
    *filesize = atoi(line);
    if(strncmp(line, "0", 1) != 0 && *filesize == 0){
        fprintf(stderr, "get_put_data: Error with atoi for filesize, got %d for \"%s\"\n", *filesize, line);
        return NULL;
    }

    memset(line, 0, MAX_LINE);
    strncpy(line, space3 + 1, space4 - space3 - 1);
    *part_no = atoi(line);
    if(strncmp(line, "0", 1) != 0 && *part_no == 0){
        fprintf(stderr, "get_put_data: Error with atoi for part_no, got %d for \"%s\"\n", *part_no, line);
        return NULL;
    }

    memset(line, 0, MAX_LINE);
    strncpy(line, space1 + 1, space2 - space1 - 1);
    if((slash = strrchr(line, '/')) != NULL){
        strcpy(filename, slash + 1);
    } else {
        strcpy(filename, line);
    }


    if(PRINTS & PRINT_METADATA)
        fprintf(stdout, "get_put_data: filename \"%s\" filesize \"%d\" part_no \"%d\"\n", 
            filename, *filesize, *part_no);

    return space3 + 1;
}

int write_put_data(int conn_fd, FILE * file, char *buf, int bytes_read, int metabytes, int bytes_left){
    int bytes_written = 0;
    bytes_read -= metabytes;
    while(1){
        // First write the data we read earlier into a file
        if(PRINTS & PRINT_DOWNLOAD_INFO) printf("bytes_written (%d) bytes_left (%d) bytes_read (%d)\n", 
            bytes_written, bytes_left, bytes_read);
        if((bytes_written = fwrite(buf + metabytes, 1, (size_t) bytes_read, file)) < 0){
            fprintf(stderr, "write_put_data: Error with fwrite\n");
            fclose(file);
            return -1;
        }
        bytes_left -= (int) bytes_written;
        if(PRINTS & PRINT_DOWNLOAD_INFO) printf("bytes_left updated (%d)\n", bytes_left);
        // If there's more data, read it again
        if(bytes_left > 0){
            memset(buf, 0, MAX_READ_SZ);
            bytes_read = read(conn_fd, buf, MAX_READ_SZ - 1);
            if(bytes_read == -1){
                fprintf(stderr, "write_put_data: ERROR with read_conn_fd\n");
                return -1;
            }
        } else {
            break;
        }
        metabytes = 0; // Remove offset from start of buffer after first write
    }
    return 0;
}

/* open_listenfd - open and return a listening socket on port
 * Returns -1 in case of failure 
 */
int open_listenfd(int port){
    int listenfd, optval=1;
    struct sockaddr_in serveraddr;
  
    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    /* listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    memset((char *) &serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons((unsigned short)port); 
    if (bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;
    return listenfd;
} /* end open_listenfd */

// Check and initialize command line arguments
int init_cmdline(int argc, char * argv[]){
    // Check for file name
    if(argc != 3 || atoi(argv[2]) < 5000 || atoi(argv[2]) > 65535){
        fprintf(stdout, "Usage \"%s <path/to/DFS/dir> <PORT>\"\n", argv[0]);
        fprintf(stdout, "where <PORT> is a value between 5000 and 65535 (inclusive)\n");
        fprintf(stderr, "init_cmdline: Incorrect command line arguments\n");
        return -1;
    }
    return 0;
}

int run_put(int conn_fd, char * cmd, char * dir, int bytes_read){
    char filename[MAX_LINE];
    char filepath[MAX_LINE * 2];
    FILE * file;
    int filesize;
    int part_no;
    int metabytes;
    int rv;

    // Extract relevent data from the get request
    if(get_get_data(cmd, filename, &part_no, &metabytes) == NULL){
        fprintf(stderr, "run_put: Error extracting metadata\n");
        return -1;
    }

    sprintf(filepath, "%s/%s/%d", dir, filename, part_no);

    // Open file
    if((file = fopen(filepath, "rb")) == NULL){
        fprintf(stderr, "run_put: ERROR opening file \"%s\"\n", filename);
        return -1;
    }

    // Get file size
    if((filesize = (int) get_file_size(file)) < 0){
        fprintf(stderr, "run_put: ERROR with getting file size for \"%s\"\n", filename);
        fclose(file);
        return -1;
    }

    char header[MAX_WRITE_SZ];
    memset(header, 0, MAX_WRITE_SZ);
    sprintf(header, "put %s %d %d ", filename, filesize, part_no);
    if(PRINTS & PRINT_SEND_INFO) printf("hdr %s\n", header);

    rv = send_string(header, conn_fd);
    if(rv == 1){
        fprintf(stderr, "run_put: Caught error with send_string, returning 0\n");
        return 0;
    } else if(rv < 0){
        fprintf(stderr, "run_put: Error with send_string\n");
        return rv;
    }

    if(send_file(file, conn_fd, filesize) < 0){
        fprintf(stderr, "Failed sending data to client\n");
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

char * get_get_data(char *buf, char *filename, int *part_no, int * metabytes){
    char * slash, * space1, * space2, * space3;
    char line[MAX_LINE];

    // Get character positions
    if((space1 = strchr(buf, ' ')) == NULL){
        fprintf(stderr, "get_put_data: Failed to parse metadata for space1\n");
        return NULL;
    }
    if((space2 = strchr(space1 + 1, ' ')) == NULL){
        fprintf(stderr, "get_put_data: Failed to parse metadata for space2\n");
        return NULL;
    }
    if((space3 = strchr(space2 + 1, ' ')) == NULL){
        fprintf(stderr, "get_put_data: Failed to parse metadata for space3\n");
        return NULL;
    }

    // Extract values
    *metabytes = space3 - buf + 1; // +1 for null-terminating character

    memset(line, 0, MAX_LINE);
    strncpy(line, space1 + 1, space2 - space1- 1);
    if((slash = strrchr(line, '/')) != NULL){
        strcpy(filename, slash + 1);
    } else {
        strcpy(filename, line);
    }

    memset(line, 0, MAX_LINE);
    strncpy(line, space2 + 1, space3 - space2 - 1);
    *part_no = atoi(line);
    if(strncmp(line, "0", 1) != 0 && *part_no == 0){
        fprintf(stderr, "get_get_data: Error with atoi for part_no, got %d for \"%s\"\n", *part_no, line);
        return NULL;
    }

    if(PRINTS & PRINT_METADATA)
        fprintf(stdout, "get_get_data: filename \"%s\" part_no \"%d\"\n", 
            filename, *part_no);

    return buf;
}
