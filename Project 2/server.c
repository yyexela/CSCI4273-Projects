#include <stdio.h>
#include <stdlib.h>
#include <string.h>      // for fgets, memset
#include <strings.h>     // for bzero, bcopy
#include <unistd.h>      // for read, write
#include <sys/socket.h>  // for socket use
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "server.h"
#include <stdint.h>      // for uintptr_t
#include <signal.h>      // for signal handling
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>  // for socket use

int main(int argc, char **argv){
    int sock_fd, *conn_fdp, port;
    unsigned int clientlen = sizeof(struct sockaddr_in);
    struct sockaddr_in client_addr;
    pthread_t tid; 

    signal(SIGPIPE, sigpipe_handler);

    // Check command line arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(0);
    }
    port = atoi(argv[1]);

    // Bind to a socket
    if((sock_fd = open_listenfd(port)) == -1){
        fprintf(stderr, "Error in creating listen socket\n");
        exit(0);
    }

    // Main loop creating threads to service requests
    while (1){
        conn_fdp = malloc(sizeof(int));
        *conn_fdp = accept(sock_fd, (struct sockaddr*)&client_addr, &clientlen);
        fcntl(*conn_fdp, F_SETFL, O_NONBLOCK); // Set socket to non-blocking
        pthread_create(&tid, NULL, thread_main, conn_fdp);
    }
}

void sigpipe_handler(int signum){
    //printf("SIGPIPE ERROR\n");
}

/* thread routine */
void *thread_main(void * vargp){
    int conn_fd = *((int *)vargp);
    char req_buf[MAX_REQ]; // Where we read into from conn_fd
    char post_buf[MAX_REQ];
    ssize_t rv;
    char *hdr = NULL; // pointer to beginning of current HTTP request header
    char *next_hdr = NULL; // pointer to beginning of next HTTP request header
    char *hdr_end = NULL;
    // Used to time out connections, initialize to current time
    struct timespec req_time;
    clock_gettime(CLOCK_MONOTONIC, &req_time);
    // Used to see if we have processed any request yet
    int use_timeout = 0;
    int keep_alive = 0;

    // Don't require the parent thread to call join to collect resources
    pthread_detach(pthread_self()); 

    if(PRINTS & PRINT_CONN_INFO) fprintf(stderr, "(%ld) read_conn_fd: Connection opened by a sender\n", pthread_self());

    while(1){
        // Get conn_fd data and put it in req_buf
        memset(req_buf, 0, MAX_REQ);
        rv = read_conn_fd(conn_fd, req_buf, MAX_REQ, &req_time, use_timeout);
        if(rv == -1){
            fprintf(stderr, "thread_main: ERROR\n");
            return NULL;
        } else if(rv == 0){
            if(PRINTS & PRINT_CONN_INFO) fprintf(stderr, "(%ld) thread_main: Connection closed by sender\n", pthread_self());
            break;
        } else if(rv == -2){
            // We timed out
            break;
        }
        use_timeout = 1; // We know for sure we have read at least once

        if(PRINTS & PRINT_DIVIDERS & PRINT_BYTES_READ) printf("------------------------\n");
        if(PRINTS & PRINT_BYTES_READ) printf("bytes read from read_conn_fd: %zd\n", rv);
        if(PRINTS & PRINT_DIVIDERS & PRINT_BYTES_READ) printf("------------------------\n");

        if(PRINTS & PRINT_DIVIDERS & PRINT_REQ_TOTAL) printf("------------------------\n");
        if(PRINTS & PRINT_LABELS & PRINT_REQ_TOTAL) printf("req_buf:\n");
        if(PRINTS & PRINT_REQ_TOTAL) printf("%s\n", req_buf);
        if(PRINTS & PRINT_DIVIDERS & PRINT_REQ_TOTAL) printf("------------------------\n");

        // Loop through each header in req_buf
        while((hdr = get_next_header(req_buf, hdr)) != NULL){
            // Extract Request Method (ex. GET/POST)
            // Extract Request URI (ex. / or /graphics/info.gif)
            // Extract Request Version (ex. HTTP/1.1)
            char req_meth[MAX_METH], req_uri[MAX_URI], req_ver[MAX_VER];
            memset(req_meth, 0, MAX_METH);
            memset(req_uri, 0, MAX_URI);
            memset(req_ver, 0, MAX_VER);
            
            // If the selected response header line is not formatted correctly
            // continue to the next request header
            if(extract_req_fields(hdr, req_meth, req_uri, req_ver)){
                continue;
            }

            // Check if there is "Connection: keep-alive"
            if(get_keep_alive(hdr) == 1){
                keep_alive = 1;
                // Set timeout 10 seconds from now
                if(PRINTS & PRINT_DIVIDERS & PRINT_TIMEOUT) printf("------------------------\n");
                if(PRINTS & PRINT_TIMEOUT) printf("(%ld) thread_main: \"keep-alive\" packet received, reset timeout\n", pthread_self());
                if(PRINTS & PRINT_DIVIDERS & PRINT_TIMEOUT) printf("------------------------\n");
                clock_gettime(CLOCK_MONOTONIC, &req_time);
            }

            if(PRINTS & PRINT_DIVIDERS & PRINT_REQ_HDR_INFO) printf("------------------------\n");
            if(PRINTS & PRINT_LABELS & PRINT_REQ_HDR_INFO) printf("req fields:\n");
            if(PRINTS & PRINT_REQ_HDR_INFO) printf("req_meth \"%s\" req_uri \"%s\" req_ver \"%s\"\n", req_meth, req_uri, req_ver);
            if(PRINTS & PRINT_DIVIDERS & PRINT_REQ_HDR_INFO) printf("------------------------\n");

            // If POST request, retreive POST DATA
            if(strncmp(req_meth, "POST", 4) == 0){
                // Find blank line before post data (that marks the start)
                hdr_end = strstr(hdr, "\r\n\r\n");

                // Find end of post data
                if((next_hdr = get_next_header(req_buf, hdr_end+5)) == NULL){
                    next_hdr = hdr_end + strlen(hdr_end);
                }
                if(hdr_end == NULL){
                    if(PRINTS & PRINT_DIVIDERS & PRINT_TIMEOUT) printf("------------------------\n");
                    if(PRINTS & PRINT_TIMEOUT) printf("(%ld) thread_main: request has no blank line, incorrect format? exiting...\n", pthread_self());
                    if(PRINTS & PRINT_DIVIDERS & PRINT_TIMEOUT) printf("------------------------\n");
                    return NULL;
                }
                // Copy post data in to its own buffer
                memcpy(post_buf, hdr_end+4, next_hdr - hdr_end + 4);

                if(handle_req(conn_fd, req_uri, req_ver, 1, post_buf, next_hdr - hdr_end) == -1){
                    fprintf(stderr, "handle_req (GET): ERROR\n");
                    return NULL;
                }
            }

            // Call function related to request method
            if(strncmp(req_meth, "GET", 3) == 0){
                // We'll be doing a GET request
                if(handle_req(conn_fd, req_uri, req_ver, 0 ,NULL, 0) == -1){
                    fprintf(stderr, "handle_req (GET): ERROR\n");
                    return NULL;
                }
            }
        }
        if(!keep_alive)
            break;
    }

    if(PRINTS & PRINT_EXIT_INFO) fprintf(stderr, "(%ld) read_conn_fd: Exiting Gracefully\n", pthread_self());

    free(vargp);
    close(conn_fd);
    return NULL;
}

// Returns 1 if "Connection: keep-alive" is found in current header, 0 otherwise
// char *hdr is the pointer to the beginning of the HTTP request
int get_keep_alive(char *hdr){
    char *hdr_end = strstr(hdr, "\r\n\r\n");
    char *next_conn = strstr(hdr, "Connection: keep-alive");

    if(next_conn == NULL || next_conn > hdr_end){
        return 0;
    }

    return 1;
}

// sets hdr to a char * to the beginning of the next HTTP command in req_buf, NULL otherwise
// also returns hdr
char *get_next_header(char *req_buf, char *hdr){
    // if hdr is NULL we know we can just use the first HTTP request
    // which is assumed to be at the start of req_buf
    if(hdr == NULL){
        hdr = req_buf;
        return hdr;
    }

    // Get char *'s to the next position of a GET and/or POST request
    char *next_get_ptr, *next_post_ptr;

    // Find next GET until end of buffer, making sure GET is at the start of req_buf or on a new line
    char *tmp = hdr;
    while((tmp = strstr(tmp + 1, "GET")) != NULL &&
        !(tmp == req_buf) && !(tmp > req_buf && (tmp-1)[0] == '\n')){}
    next_get_ptr = tmp;

    // Find next POST until end of buffer, making sure POST is at the start of req_buf or on a new line
    tmp = hdr;
    while((tmp = strstr(tmp + 1, "POST")) != NULL &&
        !(tmp == req_buf) && !(tmp > req_buf && (tmp-1)[0] == '\n')){}
    next_post_ptr = tmp;
    // Now, next_get_ptr and next_post_ptr are either NULL or valid pointers

    // Set hdr to the smallest HTTP request pointer
    if(next_get_ptr != NULL && next_post_ptr != NULL){
        hdr = next_get_ptr > next_post_ptr ? next_post_ptr : next_get_ptr;   
    } else if(next_get_ptr != NULL){
        hdr = next_get_ptr;
    } else if(next_post_ptr != NULL){
        hdr = next_post_ptr;
    } else {
        // No next command found, set hdr to NULL
        hdr = NULL;
    }

    return hdr;
}

int handle_req(int conn_fd, char *req_uri, char *req_ver, int if_post, char *post_buf, int post_len){
    // Construct a response of the format:
    //      <HTTP VERSION> <STATUS CODE> <STATUS DESCRIPTION>\r\n
    //      Content-Type: <> # Tells about the type of content and the formatting of <file contents>\r\n
    //      Content-Length:<> # Numeric value of the number of bytes of <file contents>\r\n
    //      \r\n
    //      <file contents> 
    int file_ok = 1;  // Set to 1 if we didn't get any file errors
    int status = 200; // Default status of 200 means we good G
    uint32_t file_sz = 0;
    FILE *file;
    char file_ext[MAX_EXT];
    char header[MAX_REQ];
    memset(file_ext, 0, MAX_EXT);
    memset(header, 0, MAX_REQ);

    // If URI is '/', open the file "www/index.html"
    if(strlen(req_uri) == 4 && strncmp(req_uri, "www/", 4) == 0){
        strncpy(req_uri + 4, "index.html", 11);
        if(PRINTS & PRINT_DIVIDERS) printf("------------------------\n");
        if(PRINTS & PRINT_HOME_CHANGE) printf("handle_req: File \"/\" changed to \"%s\"\n", req_uri);
        if(PRINTS & PRINT_DIVIDERS) printf("------------------------\n");
    }

    // Check if file path given by URI exists
    if(access(req_uri, R_OK) == -1){
        fprintf(stderr, "handle_req: ERROR opening file, \"%s\" doesn't exist\n", req_uri);
        file_ok = 0;
        file = NULL;
        status = 404;
    }

    // Figure out content type by the file extension
    if(file_ok && extract_file_extension(req_uri, file_ext)){
        fprintf(stderr, "handle_req: ERROR getting file extension\n");
        return -1;
    }

    if(PRINTS & PRINT_DIVIDERS) printf("------------------------\n");
    if(file_ok && PRINTS & PRINT_REQ_HDR_INFO) fprintf(stderr, "Extension is \"%s\"\n", file_ext);
    if(PRINTS & PRINT_DIVIDERS) printf("------------------------\n");

    // Open the file
    if(file_ok && (file = fopen(req_uri, "rb")) == NULL){
        printf("handle_req: ERROR opening file \"%s\"\n", req_uri);
        file_ok = 0;
        file = NULL;
    }
    
    if(file_ok && (file_sz = get_file_size(file)) == -1){
        printf("handle_req: ERROR getting file size for file \"%s\"\n", req_uri);
        file_ok = 0;
        file = NULL;
        fclose(file);
    }

    char file_type[MAX_FILE_TYPE_SZ];
    memset(file_type, 0, MAX_FILE_TYPE_SZ);

    // Construct the header
    if(strcmp(file_ext, "html") == 0 || strcmp(file_ext, "html~") == 0)
        strcpy(file_type, "text/html");
    else if(strcmp(file_ext, "css") == 0)
        strcpy(file_type, "text/css");
    else if(strcmp(file_ext, "js") == 0)
        strcpy(file_type, "application/javascript");
    else if(strcmp(file_ext, "gif") == 0)
        strcpy(file_type, "image/gif");
    else if(strcmp(file_ext, "png") == 0)
        strcpy(file_type, "image/png");
    else if(strcmp(file_ext, "jpg") == 0)
        strcpy(file_type, "image/jpg");
    else if(strcmp(file_ext, "txt") == 0)
        strcpy(file_type, "text/plain");
    if(make_header(header, req_ver, status, file_type, file_sz, if_post, post_len) == -1){
        printf("handle_req: ERROR creating header\n");
        return -1;
    }
    if(PRINTS & PRINT_DIVIDERS & PRINT_RES_HDR) printf("------------------------\n");
    if (PRINTS & PRINT_RES_HDR) printf("Response header is:\n%s\n", header);
    if(PRINTS & PRINT_DIVIDERS & PRINT_RES_HDR) printf("------------------------\n");

    if(HARDCODED_RES) hardcoded_response(conn_fd);
    else{
        if(send_response(conn_fd, header, file, file_sz, if_post, post_buf, post_len) == -1){
            fprintf(stderr, "handle_req: ERROR with send_response\n");
            return -1;
        }
    }

    if(PRINTS & PRINT_DIVIDERS & PRINT_WRITE_DONE) printf("------------------------\n");
    if(PRINTS & PRINT_WRITE_DONE) printf("Finished sending %s\n", req_uri);
    if(PRINTS & PRINT_DIVIDERS & PRINT_WRITE_DONE) printf("------------------------\n");

    if(file != NULL) fclose(file);

    return 0;
}

int clean_write(int fd, void *buf, size_t n){
    ssize_t bytes = 0;
    if((bytes = write(fd, buf, n)) <= 0){
        if(errno == EPIPE){
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

int send_response(int conn_fd, char *header, FILE *file, uint32_t file_sz, int if_post, char* post_buf, int post_len){
    int rv = 0;
    int in_body_tag = 0;

    // write the header
    size_t bytes_left = (size_t) strlen(header); // bytes left in the header to send
    size_t bytes_written = 0; // bytes written from the header
    while(bytes_left > 0){
        size_t bytes_no = bytes_left > MAX_WRITE_SZ ? MAX_WRITE_SZ : bytes_left; // bytes to write this call
        rv = clean_write(conn_fd, header + (bytes_written), bytes_no);
        if(rv == 0){
            return 0;
        } else if (rv < 0){
            fprintf(stderr, "send_response: ERROR with clean_write\n");
            return rv;
        }
        bytes_left -= bytes_no;
        bytes_written += bytes_no;
    }

    // Check if file exists
    if(file == NULL) return 0;

    // write the file
    char file_buf[MAX_WRITE_SZ];
    char *file_index = file_buf;
    bytes_left = (size_t) file_sz; // bytes left in the file to send
    bytes_written = (size_t) 0; // bytes written from the file

    if(if_post){
        while(bytes_left > 0){
            // Read bytes from file one at a time in to file buffer
            if(fread((void *) file_index, (size_t) 1, 1, file) == 0){
                fprintf(stderr, "send_response: ERROR with fread (file_sz %u, bytes_no %d)\n", file_sz, 1);
                return -1;
            }
            file_index += 1;
            bytes_left -= 1;
            bytes_written += 1;

            // If the last 4 bytes read were "body" note that we are in the body tag
            if(strncmp(file_index-4, "body", 4) == 0){
                in_body_tag = 1;
            }
            // If we were already in the body tag and come across the end of it, 
            // send the file buf so far and the post data
            if(in_body_tag && strncmp(file_index-1, ">", 1) == 0){
                // Write the file buffer read so far into the socket
                rv = clean_write(conn_fd, file_buf, bytes_written);
                if (rv < 0){
                    fprintf(stderr, "send_response: ERROR with clean_write\n");
                    return rv;
                }

                // Write the post prefix into the socket
                rv = clean_write(conn_fd, "<pre><h1>", 9);
                if (rv < 0){
                    fprintf(stderr, "send_response: ERROR with clean_write\n");
                    return rv;
                }

                // Write the post data into the socket
                rv = clean_write(conn_fd, post_buf, post_len);
                if (rv < 0){
                    fprintf(stderr, "send_response: ERROR with clean_write\n");
                    return rv;
                }

                // Write the post suffix into the socket
                rv = clean_write(conn_fd, "</h1></pre>", 11);
                if (rv < 0){
                    fprintf(stderr, "send_response: ERROR with clean_write\n");
                    return rv;
                }
                break;
            }
        }
    }

    // For the rest of the data in the file (or the entire file for GET requests)
    // read the file in chunks in to a file buffer and then write that file buffer
    // into the socket repeatedly until EoF
    while(bytes_left > 0){
        size_t bytes_no = bytes_left > MAX_WRITE_SZ ? MAX_WRITE_SZ : bytes_left; // bytes to write this call
        if(fread((void *) file_buf, (size_t) 1, bytes_no, file) == 0){
            fprintf(stderr, "send_response: ERROR with fread (file_sz %u, bytes_no %lu)\n", file_sz, bytes_no);
            return -1;
        }
        rv = clean_write(conn_fd, file_buf, bytes_no);
        if(rv == 0){
            return 0;
        } else if (rv < 0){
            fprintf(stderr, "send_response: ERROR with clean_write\n");
            return rv;
        }
        bytes_left -= bytes_no;
        bytes_written += bytes_no;
    }

    return 0;
}

int make_header(char *header, char *req_ver, int status, char * type, uint32_t file_sz, int if_post, int post_len){
    // Make string from status and file_sz
    char status_str[MAX_STATUS];
    memset(status_str, 0, MAX_STATUS);
    sprintf(status_str, "%d", status);

    char file_sz_str[MAX_FILE_SZ];
    memset(file_sz_str, 0, MAX_FILE_SZ);
    sprintf(file_sz_str, "%u", file_sz + if_post*(post_len + 20));

    uint32_t pos = 0; // Cursor position of header while we construct it
    strcpy(header, req_ver);
    pos += strlen(req_ver);
    header[pos] = ' ';
    pos++;
    strcpy(header + pos, status_str);
    pos += strlen(status_str);

    // Insert message based on status code
    if(status == 200){
        header[pos] = ' ';
        pos++;
        strcpy(header + pos, "Document Follows");
        pos += strlen("Document Follows");
    } else if(status == 404){
        header[pos] = ' ';
        pos++;
        strcpy(header + pos, "File Doesn't Exist");
        pos += strlen("File Doesn't Exist");
    } else if(status == 500){
        header[pos] = ' ';
        pos++;
        strcpy(header + pos, "Internal Server Error");
        pos += strlen("Internal Server Error");
    } else {
        // Don't add content type or anything else
        return 0;
    }

    header[pos] = '\r';
    pos++;
    header[pos] = '\n';
    pos++;
    strcpy(header + pos, "Content-Type:");
    pos += strlen("Content-Type:");
    strcpy(header + pos, type);
    pos += strlen(type);
    header[pos] = '\r';
    pos++;
    header[pos] = '\n';
    pos++;
    strcpy(header + pos, "Content-Length:");
    pos += strlen("Content-Length:");
    strcpy(header + pos, file_sz_str);
    pos += strlen(file_sz_str);

    header[pos] = '\r';
    pos++;
    header[pos] = '\n';
    pos++;
    header[pos] = '\r';
    pos++;
    header[pos] = '\n';
    pos++;

    return 0;
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

int extract_file_extension(char *req_uri, char  *file_ext){
    char *loc; // Pointer to period of the file extensions
    if ((loc = strrchr(req_uri, '.')) == NULL){
        fprintf(stderr, "extract_file_extension: ERROR finding last period in URI\n");
        return -1;
    }
    uintptr_t dot = loc - req_uri;
    strncpy(file_ext, req_uri + dot + 1, strlen(req_uri) - dot - 1);
    return 0;
}

/*
 * echo - echo text lines until client closes connection
 */
void hardcoded_response(int conn_fd){
    char * httpmsg="HTTP/1.1 200 Document Follows\r\nContent-Type:text/html\r\nContent-Length:32\r\n\r\n<html><h1>Hello CSCI4273 Course!</h1>"; 
    printf("server returning a http message with the following content.\n%s\n", httpmsg);
    for(int i = 0; i < strlen(httpmsg); i++){
        write(conn_fd, httpmsg + i, 1);
    }
}

/* 
 * open_listenfd - open and return a listening socket on port
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

int extract_req_fields(char *req_buf, char *req_meth, char *req_uri, char *req_ver){
    // Expect format of <REQ_METH>" "<REQ_URI>" "<REQ_VER>\r\n
    char *loc; // Location of character got from strchr

    // Get the positions of field delimeters
    if ((loc = strchr(req_buf, ' ')) == NULL){
        fprintf(stderr, "extract_req_fields: ERROR finding first space in request\n");
        return -1;
    }
    uintptr_t space_1 = loc - req_buf;
    if ((loc = strchr(req_buf + space_1 + 1, ' ')) == NULL){
        fprintf(stderr, "extract_req_fields: ERROR finding second space in request\n");
        return -1;
    }
    uintptr_t space_2 = loc - req_buf;
    if ((loc = strchr(req_buf, '\r')) == NULL){
        fprintf(stderr, "extract_req_fields: ERROR finding end of first line in request\n");
        return -1;
    }
    uintptr_t line_end = loc - req_buf;
    if(space_1 >= space_2 || space_1 >= line_end || space_2 >= line_end){
        fprintf(stderr, "extract_req_fields: ERROR finding the end of the first line in request\n");
        return -1;
    }

    // Prepend req_uri with "www" to work with path searches
    // NOTE: if the # of things we copy is 3 we get a compiler error saying the \0 isn't copied
    strncpy(req_uri, "www", 4);

    // Fill in the request fields
    strncpy(req_meth, req_buf, space_1);
    strncpy(req_uri + 3, req_buf + space_1 + 1, space_2 - space_1 - 1);
    strncpy(req_ver, req_buf + space_2 + 1, line_end - space_2 - 1);
    return 0;
}

ssize_t read_conn_fd(int conn_fd, char *buf, int n, struct timespec *req_time, int use_timout){
    ssize_t n_read;  // # of bytes read from conn_fd
    struct timespec curr_time; // current time
    while((n_read = read(conn_fd, buf, MAX_REQ)) == -1){
        if(errno == EAGAIN || errno == EWOULDBLOCK){
            clock_gettime(CLOCK_MONOTONIC, &curr_time);
            // Make sure that this is a normal -1
            // Check timeout
            if(use_timout && req_time->tv_sec + ((time_t) CONN_TIMEOUT) <= curr_time.tv_sec && req_time->tv_nsec <= curr_time.tv_nsec){
                if(PRINTS & PRINT_DIVIDERS & PRINT_TIMEOUT) printf("------------------------\n");
                if(PRINTS & PRINT_TIMEOUT) printf("(%ld) read_conn_fd: timed out after %ld.%.9ld\n", pthread_self(), curr_time.tv_sec - req_time->tv_sec, curr_time.tv_nsec - req_time->tv_nsec);
                if(PRINTS & PRINT_DIVIDERS & PRINT_TIMEOUT) printf("------------------------\n");
                return -2;
            }
            continue;
        } else if(errno == EPIPE){
            fprintf(stderr, "read_conn_fd: ERROR with read, caught EPIPE\n");
            return 0;
        } else if(errno == 104){
            fprintf(stderr, "read_conn_fd: ERROR with read, caught \"Connnection reset by peer\"\n");
            return 0;
        } else {
            fprintf(stderr, "read_conn_fd: ERROR reading from conn_fd\n");
            perror("");
            return -1;
        }
    }
    return n_read;
}
