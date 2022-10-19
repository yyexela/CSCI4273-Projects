/* 
 * Question to ask prof:
 * - Canvas discussion thread?
 *   - He will make one, we can also make one in the future.
 * - If we're using elra, will we only need to worry about `nc` or `telnet`, if not how do we
 *   use a browser on it?
 *   - Will probably test via telnet. Can also run proxy on elra machine and run client web-browser
 *     locally (connecting to VPN). Can also use X-Windows (XQuartz) and run `ssh -Y ...` (something
 *     similar on windows).
 * - Err 400 or 404 when hostname isn't resolved?
 *   - Allows either, specify in README
 * - What should we do with the client connections? Timeout, wait for them to close, or close them?
 *   - Wait for client to close it (they're probably not testing it).
 * - Do we need to extract the host name from the second field in the first line of the HTTP request?
 *   browser automatically adds Host: field but this is not what's used in examples in the write-up.
 *   - Try other web-browsers and see what they do.
 * - What do we do with the server connections? Timeout, wait for them to close, or close them?
 *   - Same as client above
 * - What if there's no content-length in the header? ie (http://httpforever.com/)
 *   - Assume content-length is in the header
 * - How to test server?
 *   - Browsing old-school text-based webpages is good.
 * - Telnet doesn't instantly send \r\n\r\n, cases problems
 *   - Probably handle the case
 * - MD5, OK to use popen? Executes shell...
 *   - It's ok as long as it works on the elra machines. Looka at openssl md5.h.
 * 
 * TO ASK:
 * - getaddrinfo can still make a DNS call... do we want to avoid using that function if we make
 *   calls to gethostbyname?
 *   - It's alright
 * - Ask about mutexes
 *   - Will probably have to use them
 * 
 * TO DO:
 * - Blacklist
 * - Fix when threads close
 */

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

pthread_mutex_t ll_lock;
pthread_mutex_t cache_lock;
pthread_mutex_t resolved_lock;
node *head = NULL;
time_t cache_timeout = (time_t) 10; // cache timeout in seconds

int main(int argc, char **argv){
    int sock_fd, *conn_fdp, port;
    unsigned int clientlen = sizeof(struct sockaddr_in);
    struct sockaddr_in client_addr;
    pthread_t tid; 

    // Initialize mutexes
    // Note: this will leak memory since we don't call pthread_mutex_destroy()
    pthread_mutex_init(&ll_lock, NULL);
    pthread_mutex_init(&cache_lock, NULL);
    pthread_mutex_init(&resolved_lock, NULL);

    signal(SIGPIPE, sigpipe_handler);

    // Check command line arguments
    if(init_cmdline(argc, argv, &port, &cache_timeout) < 0)
        exit(0);
    // TODO: Copy command line arguments check from earlier PA

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

// Check and initialize command line arguments
int init_cmdline(int argc, char * argv[], int * port, time_t * cache_timeout) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s <port> <cache_timeout (int seconds)>\n", argv[0]);
    return -1;
  }
  *port = atoi(argv[1]);
  *cache_timeout = atoi(argv[2]);

  if (*port < 5000){
    fprintf(stderr, "<port> must not be less than 5000, not %d\n", *port);
    return -1;
  }
  if(*cache_timeout <= 0){
    fprintf(stderr, "<cache_timeout (int seconds)> must be greater than 0, not %d\n", *port);
    return -1;
  }
  
  return 0;
}

void sigpipe_handler(int signum){
    //printf("SIGPIPE ERROR\n");
}

ssize_t obtain_header(int *conn_fd, char *buf, int max_buf){
    ssize_t rv;
    ssize_t bytes_read = 0;

    // Keep reading until we get a complete header, that is, we have an \r\n\r\n
    while(1){
        // Make sure there's still room in the buffer
        if(max_buf - 1 - bytes_read <= 0){
            fprintf(stderr, "(%ld) obtain_header: ERROR header too big\n", pthread_self());
            return -1;
        }

        rv = read_conn_fd(conn_fd, buf + bytes_read, max_buf - 1 - bytes_read); // Make sure there's a '\0' at the end
        if(rv == -1){
            fprintf(stderr, "(%ld) obtain_header: ERROR with read_conn_fd\n", pthread_self());
            return -1;
        } else if(rv == 0){
            fprintf(stderr, "(%ld) obtain_header: ERROR connection closed by sender\n", pthread_self());
            return 0;
        } else if(rv == -2){
            // We timed out
            return -2;
        }

        bytes_read += rv;
        // Check for \r\n\r\n
        if (strstr(buf, "\r\n\r\n") == NULL){
            // Not found, wait for next read
            continue;
        } else {
            // Found, continue processing
            break;
        }
    }
    return bytes_read;
}

void clean_exit(void *vargp, int conn_fd){
    if(PRINTS & PRINT_EXIT_INFO) fprintf(stderr, "(%ld) read_conn_fd: Exiting Gracefully\n", pthread_self());
    free(vargp);
    close(conn_fd);
}

/* thread routine */
void *thread_main(void *vargp){
    int conn_fd = *((int *)vargp);
    char req_buf[MAX_REQ]; // Where we read into from conn_fd
    ssize_t rv; // Used to store return values and make code cleaner
    char *hdr = NULL; // pointer to beginning of current HTTP request header

    // Don't require the parent thread to call join to collect resources
    pthread_detach(pthread_self()); 

    if(PRINTS & PRINT_DIVIDERS & PRINT_CONN_INFO) printf("------------------------\n");
    if(PRINTS & PRINT_CONN_INFO) fprintf(stderr, "(%ld) Connection opened by a sender\n", pthread_self());
    if(PRINTS & PRINT_DIVIDERS & PRINT_CONN_INFO) printf("------------------------\n");

    while(1){
        // Get conn_fd data and put it in req_buf
        memset(req_buf, 0, MAX_REQ);
        rv = obtain_header(&conn_fd, req_buf, MAX_REQ);
        if(rv == -1){
            fprintf(stderr, "(%ld) thread_main: ERROR with read_conn_fd\n", pthread_self());
            clean_exit(vargp, conn_fd);
            return NULL;
        } else if(rv == 0){
            if(PRINTS & PRINT_CONN_INFO) fprintf(stderr, "(%ld) thread_main: Connection closed by sender\n", pthread_self());
            clean_exit(vargp, conn_fd);
            return NULL;
        } else if(rv == -2){
            // We timed out
            clean_exit(vargp, conn_fd);
            return NULL;
        }

        if(PRINTS & PRINT_DIVIDERS & PRINT_BYTES_READ) printf("------------------------\n");
        if(PRINTS & PRINT_BYTES_READ) printf("(%ld) bytes read from read_conn_fd: %zd\n", pthread_self(), rv);
        if(PRINTS & PRINT_DIVIDERS & PRINT_BYTES_READ) printf("------------------------\n");

        if(PRINTS & PRINT_DIVIDERS & PRINT_REQ_TOTAL) printf("------------------------\n");
        if(PRINTS & PRINT_LABELS & PRINT_REQ_TOTAL) printf("(%ld) req_buf:\n", pthread_self());
        if(PRINTS & PRINT_REQ_TOTAL) printf("Client request:\n%s\n", req_buf);
        if(PRINTS & PRINT_DIVIDERS & PRINT_REQ_TOTAL) printf("------------------------\n");
        // Loop through each header in req_buf

        while((hdr = get_next_header(req_buf, hdr)) != NULL){

            // Extract Request Method (ex. GET)
            // Extract Request URI (ex. / or /graphics/info.gif)
            // Extract Request Version (ex. HTTP/1.1)
            // Extract Request Host + Request Port (ie. google.com:8080)
            char req_meth[MAX_METH], req_uri[MAX_URI], req_ver[MAX_VER], req_host[MAX_HOST], req_port[MAX_PORT], req_ip[MAX_URI];
            memset(req_meth, 0, MAX_METH); memset(req_uri, 0, MAX_URI);   memset(req_ver, 0, MAX_VER);
            memset(req_host, 0, MAX_HOST); memset(req_port, 0, MAX_PORT); memset(req_ip, 0, MAX_URI);
            
            // If the selected response header line is not formatted correctly
            // continue to the next request header
            if(extract_req_fields(hdr, req_meth, req_uri, req_ver, req_host, req_port)){
                continue;
            }

            if(PRINTS & PRINT_DIVIDERS & PRINT_REQ_HDR_INFO) printf("------------------------\n");
            if(PRINTS & PRINT_LABELS & PRINT_REQ_HDR_INFO) printf("(%ld) req fields:\n", pthread_self());
            if(PRINTS & PRINT_REQ_HDR_INFO) printf("(%ld) req_meth \"%s\" req_uri \"%s\" req_ver \"%s\" req_host \"%s\" req_port \"%s\"\n", pthread_self(), req_meth, req_uri, req_ver, req_host, req_port);
            if(PRINTS & PRINT_DIVIDERS & PRINT_REQ_HDR_INFO) printf("------------------------\n");

            // Call function related to request method
            if(strncmp(req_meth, "GET", 3) == 0){
                // Check in DNS lookup cache
                if(get_ip(req_host, req_ip) < 0){
                    if(PRINTS & PRINT_DNS_RESULTS) printf("(%ld) Couldn't find \"%s\"\n", pthread_self(), req_host);
                    // Perform DNS lookup
                    if(check_hostname(req_host, req_ip) < 0){
                        send_error(conn_fd, 400, req_ver);
                        continue;
                    }
                    save_ip(req_host, req_ip);
                } else {
                    if(PRINTS & PRINT_DNS_RESULTS) printf("(%ld) Returned \"%s\" for \"%s\"\n", pthread_self(), req_ip, req_host);
                }

                // Check if hostname or IP is blacklisted, return error to client if so
                if(check_blacklist(req_host, req_ip)){
                    if(PRINT_BLACKLIST) printf("(%ld) Hostname: \"%s\", IP: \"%s\", is blacklisted, returning error to client\n", pthread_self(), req_host, req_ip);
                    send_error(conn_fd, 403, req_ver);
                    continue;
                }

                // We'll be doing a GET request
                if(handle_req(conn_fd, req_uri, req_ver, req_ip, req_port, hdr) == -1){
                    fprintf(stderr, "(%ld) thread_main: handle_req (GET): ERROR\n", pthread_self());
                    clean_exit(vargp, conn_fd);
                    return NULL;
                }
            } else {
                fprintf(stderr, "(%ld) thread_main: Method \"%s\" not supported\n", pthread_self(), req_meth);
                send_error(conn_fd, 405, req_ver);
            }
        }
    }
    
    if(PRINTS & PRINT_EXIT_INFO) fprintf(stderr, "(%ld) read_conn_fd: Exiting Gracefully\n", pthread_self());

    clean_exit(vargp, conn_fd);
    return NULL;
}

int check_hostname(char *req_host, char *req_ip){
    // Perform DNS lookup for req_host
    struct hostent *host = gethostbyname(req_host);
    if(host == NULL){
        // Host not found
        fprintf(stderr, "(%ld) thread_main: Host \"%s\" not recognized\n", pthread_self(), req_host);
        return -1;
    }

    if(PRINTS & PRINT_DNS_RESULTS && host != NULL){
        if(PRINT_DIVIDERS) printf("------------------------\n");
        printf("(%ld) Host \"%s\" resolved as \"%hhu.%hhu.%hhu.%hhu\"\n", pthread_self(), req_host, host->h_addr_list[0][0], host->h_addr_list[0][1], host->h_addr_list[0][2], host->h_addr_list[0][3]);
        if(PRINT_DIVIDERS) printf("------------------------\n");
    }

    sprintf(req_ip, "%hhu.%hhu.%hhu.%hhu", host->h_addr_list[0][0], host->h_addr_list[0][1], host->h_addr_list[0][2], host->h_addr_list[0][3]);

    return 0;
}

int send_error(int conn_fd, int status, char *req_ver){
    // Response header
    char header[MAX_RES];
    memset(header, 0, MAX_RES);
    make_error_header(header, status, req_ver);
    send_data(conn_fd, header, NULL, 0);
    return 0;
}

int make_error_header(char *header, int status, char *req_ver){
    // Make string from status and file_sz
    char status_str[MAX_STATUS];
    memset(status_str, 0, MAX_STATUS);
    sprintf(status_str, "%d", status);

    uint32_t pos = 0; // Cursor position of header while we construct it
    strcpy(header, req_ver);
    pos += strlen(req_ver);
    header[pos] = ' ';
    pos++;
    strcpy(header + pos, status_str);
    pos += strlen(status_str);

    // Insert message based on status code
    if(status == 400){
        header[pos] = ' ';
        pos++;
        strcpy(header + pos, "Host Not Found");
        pos += strlen("Host Not Found");
    } else if(status == 403){
        header[pos] = ' ';
        pos++;
        strcpy(header + pos, "Forbidden");
        pos += strlen("Forbidden");
    } else if(status == 404){
        header[pos] = ' ';
        pos++;
        strcpy(header + pos, "File Doesn't Exist");
        pos += strlen("File Doesn't Exist");
    } else if(status == 405){
        header[pos] = ' ';
        pos++;
        strcpy(header + pos, "Method Not Supported");
        pos += strlen("Method Not Supported");
    } else if(status == 500){
        header[pos] = ' ';
        pos++;
        strcpy(header + pos, "Internal Server Error");
        pos += strlen("Internal Server Error");
    } else {
        // Don't add content type or anything else
        strcpy(header + pos, "\r\n\r\n");
        pos += strlen("\r\n\r\n");
        return 0;
    }

    strcpy(header + pos, "\r\n\r\n");
    pos += strlen("\r\n\r\n");

    return 0;
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

int init_server_tcp(int *server_fd, struct addrinfo *hints, struct addrinfo *serverinfo, char *req_host, char *req_port){
    if(PRINTS & PRINT_SERVER_CONN) printf("(%ld) Will try to connect to %s:%s\n", pthread_self(), req_host, req_port);

    int status;
    struct addrinfo *p;

    memset(hints, 0, sizeof(*hints));
    hints->ai_family = AF_UNSPEC;      // IPV4 or IPV6
    hints->ai_socktype = SOCK_STREAM;  // TCP

    if((status = getaddrinfo(req_host, req_port, hints, &serverinfo)) != 0){
        fprintf(stderr, "(%ld) init_server_tcp: Error in getaddrinfo\n", pthread_self());
        return -1;
    }

    for(p = serverinfo; p != NULL; p = p->ai_next){
        // if(PRINTS & PRINT_SERVER_CONN) printf("(%ld) Trying to connect to %s\n", pthread_self(), p->ai_canonname);

        if((*server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            fprintf(stderr, "(%ld) init_server_tcp: Error in socket\n", pthread_self());
            continue;
        }

        if((status = connect(*server_fd, p->ai_addr, p->ai_addrlen)) == -1){
            fprintf(stderr, "(%ld) init_server_tcp: Error in connect, %d\n", pthread_self(), status);
            close(*server_fd);
            continue;
        }

        break;
    }

    if (p == NULL){
        fprintf(stderr, "(%ld) init_server_tcp: Error connecting to \"%s:%s\"\n", pthread_self(), req_host, req_port);
        freeaddrinfo(serverinfo);
        return -1;
    }

    fcntl(*server_fd, F_SETFL, O_NONBLOCK); // Set socket to non-blocking

    return 0;
}

int get_md5(char *req_uri, char *ret){
    // Set return value to empty string
    memset(ret, '\0', 33);

    //printf("(%ld) \n", pthread_self());
    if(PRINTS & PRINT_MD5) printf("(%ld) get_md5: Converting %s\n", pthread_self(), req_uri);

    // Create cmd string
    size_t uri_len = strlen(req_uri);
    char *cmd = malloc(uri_len + 17);
    strcpy(cmd, "echo \'");
    strcpy(cmd + 6, req_uri);
    strcpy(cmd + 6 + uri_len, "\' | md5sum");
    
    if(PRINTS & PRINT_MD5) printf("(%ld) get_md5: Calling %s\n", pthread_self(), cmd);

    FILE *cmd_response;

    if((cmd_response = popen(cmd, "r")) == NULL){
        fprintf(stderr, "(%ld) get_md5: ERROR calling \"popen\" with command \"%s\"\n", pthread_self(), cmd);
        return -1;
    }

    size_t pos = 0;
    size_t bytes_read = 0;
    size_t bytes_left = 32;
    while(pos != 32){
        if((bytes_read = fread(ret + pos, 1, bytes_left, cmd_response)) < 0){
            fprintf(stderr, "(%ld) get_md5: ERROR calling \"fread\"\n", pthread_self());
            return -1;
        }
        pos += bytes_read;
    }

    if(strlen(ret) > 32){
        fprintf(stderr, "(%ld) get_md5: ERROR returned more than 32 bytes\n", pthread_self());
        return -1;
    }

    if(pclose(cmd_response)){
        fprintf(stderr, "get_md5: ERROR with pclose\n");
    }

    if(PRINTS & PRINT_MD5) printf("(%ld) get_md5: Returning cache/%s\n", pthread_self(), ret);

    return 0;
}

int check_blacklist(char* hostname, char* ip){
    FILE* file;
    char list_line[256];

    if((file = fopen("blacklist.txt", "r")) == NULL){
        fprintf(stderr, "File `blacklist.txt` cannot be opened\n");
        return -1;
    }

    while(fgets(list_line, sizeof(list_line), file)){
        // Overwrite the newline/EOF at the end of each line with null char so we
        // just have the data
        list_line[strlen(list_line)-1] = '\0';
        if(!strcmp(hostname, list_line) || !strcmp(ip, list_line)){
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0;
}

// Receive a file from the socket `server_fd`. `filename` saves the name of the file
int recv_file(int *server_fd, FILE *file, char *req_host, char *req_port, int *file_size){
    int bytes_read = 0, bytes_left = 0, bytes_written = 0;
    uintptr_t hdr_sz = 0;

    // Get HTTP header in buffer and get Content-Length
    char buf[MAX_RES];
    memset(&buf, 0, sizeof(buf));

    // Ensure the entire HTTP header is in the buffer first
    bytes_read = obtain_header(server_fd, buf, MAX_RES);
    if(bytes_read == -1){
        fprintf(stderr, "(%ld) recv_file: ERROR with obtain_header\n", pthread_self());
        return -1;
    } else if(bytes_read == 0){
        if(PRINTS & PRINT_CONN_INFO) fprintf(stderr, "(%ld) recv_file: Connection closed by sender\n", pthread_self());
        return -1;
    } else if(bytes_read == -2){
        // We timed out
        fprintf(stderr, "(%ld) recv_file: ERROR with obtain_header, timed out from server\n", pthread_self());
        return -1;
    }

    if(PRINTS & PRINT_SEND_INFO) printf("Server header:\n%s\n", buf);

    // Parse for `Content-Length:` and extract its value, set to '0' otherwise
    // For the PA we can assume Content-Length will always exist, the '0' case is for
    // browser testing
    char content_length[MAX_CONTENT_SZ];
    if(get_content_length(buf, content_length) < 0){
        if(PRINTS) fprintf(stderr, "(%ld) recv_file: Error getting Content-Length value, setting to 0\n", pthread_self());
        content_length[0] = '0';
    }
    if(PRINTS & PRINT_CONTENT_SZ) printf("(%ld) recv_file: content-size for file is \"%s\"\n", pthread_self(), content_length);

    // Get the length of the header
    if(get_header_size(buf, &hdr_sz) < 0){
        fprintf(stderr, "(%ld) recv_file: Error getting header size\n", pthread_self());
        return -1;
    }
    if(PRINTS & PRINT_CONTENT_SZ) printf("(%ld) recv_file: header size is \"%lu\" of a total of \"%lu\" read\n", pthread_self(), hdr_sz, strlen(buf));

    // Place data we initially read into file
    bytes_left = atoi(content_length) + (int) hdr_sz;
    *file_size = bytes_left; // Updated file size value once we know it, used outside the function
    while(1){
        // First write the data we read earlier into a file
        if(PRINTS & PRINT_WRITE_INFO) printf("(%ld) bytes_written (%d) bytes_left (%d) bytes_read (%d)\n", pthread_self(), bytes_written, bytes_left, bytes_read);
        if((bytes_written = fwrite(buf, 1, bytes_read, file)) < 0){
            fprintf(stderr, "(%ld) recv_file: Error with fwrite\n", pthread_self());
            fclose(file);
            return -1;
        }
        bytes_left -= (int) bytes_written;
        if(PRINTS & PRINT_WRITE_INFO) printf("(%ld) bytes_left updated (%d)\n", pthread_self(), bytes_left);
        // If there's more data, read it again
        if(bytes_left > 0){
            memset(buf, 0, MAX_RES);
            bytes_read = read_conn_fd(server_fd, buf, MAX_RES - 1); // Make sure there's a '\0' at the end
            if(bytes_read == -1){
                fprintf(stderr, "(%ld) recv_file: ERROR with read_conn_fd\n", pthread_self());
                return -1;
            } else if(bytes_read == 0){
                fprintf(stderr, "(%ld) recv_file: ERROR connection closed by sender\n", pthread_self());
                return -1;
            } else if(bytes_read == -2){
                // We timed out
                return -1;
            }
        } else {
            break;
        }
    }

    if(PRINTS & PRINT_SEND_INFO) printf("Received data from HTTP server:\n%s\n", buf);

    return 0;
}

int download_file(FILE **file, char *file_path, char *hdr, char *req_host, char *req_port, char *req_uri, int *file_size){
    int server_fd;
    struct addrinfo hints;
    struct addrinfo *serverinfo = NULL;

    // Initiate TCP connection with the server
    if(init_server_tcp(&server_fd, &hints, serverinfo, req_host, req_port) < 0){
        fprintf(stderr, "(%ld) download_file: Error initializing TCP connection to \"%s:%s\"\n", pthread_self(), req_host, req_port);
    }

    // Forward HTTP header from client to server
    if(PRINTS & PRINT_SERVER_CONN) printf("(%ld) download_file: Forwarding header\n", pthread_self());

    if(send_data(server_fd, hdr, NULL, 0) != 0){
        fprintf(stderr, "(%ld) download_file: Error sending header to \"%s:%s\"\n", pthread_self(), req_host, req_port);
    }

    // Open the file
    *file = fopen(file_path, "wb+");
    if(*file == NULL){
        fprintf(stderr, "(%ld) download_file: Error in opening file for \"%s\"\n", pthread_self(), req_uri);
        close(server_fd);
        freeaddrinfo(serverinfo);
        return -1;
    }

    // Save response file locally and record file size
    if(recv_file(&server_fd, *file, req_host, req_port, file_size) < 0){
        fprintf(stderr, "(%ld) download_file: Error in recv_file for \"%s\"\n", pthread_self(), req_uri);
        freeaddrinfo(serverinfo);
        close(server_fd);
        return -1;
    }

    // Return to start of file
    if(fseek(*file, 0, SEEK_SET) < 0){
        fprintf(stderr, "(%ld) download_file: ERROR with fseek-ing to the start of the file\n", pthread_self());
        freeaddrinfo(serverinfo);
        close(server_fd);
        return -1;
    }
    close(server_fd);
    return 0;
}

int handle_req(int conn_fd, char *req_uri, char *req_ver, char *req_host, char *req_port, char *hdr){
    // Initiate HTTP server TCP connection
    FILE *file;
    int file_size;
    node *ll_node;

    // Get MD5 string from req_uri (cached file name)
    char file_path[39]; // 'cache/' + md5 (32 bytes) + '\0' for 39 bytes
    strcpy(file_path, "cache/");
    if(get_md5(req_uri, file_path+6) < 0){
        fprintf(stderr, "(%ld) handle_req: Error in md5 for \"%s\"\n", pthread_self(), req_uri);
    }

    if(PRINTS & PRINT_MD5) printf("(%ld) handle_req: md5 for \"%s\" is \"%s\"\n", pthread_self(), req_uri, file_path);

    // Check if file already exists and it has not expired
    if((ll_node = check_node(file_path)) != NULL){
        // File exists in LL, check if it has timed out
        if(check_timeout(ll_node)){
            if(PRINTS & PRINT_CACHE_INFO) printf("(%ld) %s CACHE MISS IN LL (OLD FILE TIMED OUT)\n", pthread_self(), file_path);
            // Node has timed out, delete it and download file
            delete_node(ll_node);
            pthread_mutex_lock(&cache_lock);
            if(download_file(&file, file_path, hdr, req_host, req_port, req_uri, &file_size) < 0){
                fprintf(stderr, "(%ld) handle_req: ERROR download_file\n", pthread_self());
            }
            // Add to LL
            ll_node = create_node();
            update_time(ll_node);
            update_name(ll_node, file_path);
            add_node(ll_node);
        } else {
            if(PRINTS & PRINT_CACHE_INFO) printf("(%ld) %s CACHE HIT IN LL\n", pthread_self(), file_path);
            // Node has not timed out, we have a cache hit
            // Open file and get its size
            pthread_mutex_lock(&cache_lock);
            file = fopen(file_path, "rb");
            if((file_size = (int) get_file_size(file)) < 0){
                fprintf(stderr, "(%ld) handle_req: ERROR with getting file size of cached file\n", pthread_self());
            }
        }
    } else {
        if(PRINTS & PRINT_CACHE_INFO) printf("(%ld) %s CACHE MISS NOT IN LL\n", pthread_self(), file_path);
        // File isn't in linked list, download it
        pthread_mutex_lock(&cache_lock);
        if(download_file(&file, file_path, hdr, req_host, req_port, req_uri, &file_size) < 0){
            fprintf(stderr, "(%ld) handle_req: ERROR download_file\n", pthread_self());
        }
        // Add to LL
        ll_node = create_node();
        update_time(ll_node);
        update_name(ll_node, file_path);
        add_node(ll_node);
    }

    if(PRINTS & PRINT_SEND_INFO) printf("(%ld) Sending file \"%s\" of size %u to client\n", pthread_self(), req_uri, file_size);

    if(send_data(conn_fd, NULL, file, (uint32_t) file_size) != 0){
        fprintf(stderr, "(%ld) handle_req: Error sending data from \"%s:%s\" to client\n", pthread_self(), req_host, req_port);
        pthread_mutex_unlock(&cache_lock);
        return -1;
    }

    fclose(file);
    pthread_mutex_unlock(&cache_lock);
    return 0;
}

int clean_write(int fd, void *buf, size_t n){
    //Remove O_NONBLOCK
    int val = fcntl(fd, F_GETFL, 0);
    int flags = O_NONBLOCK;
    val &= ~flags;
    fcntl(fd,F_SETFL,val);  

    ssize_t bytes = 0;
    if((bytes = write(fd, buf, n)) <= 0){
	if(errno == EAGAIN){
            fprintf(stderr, "(%ld) clean_write: ERROR with write, caught EAGAIN\n", pthread_self());
    	} else if(errno == EPIPE){
            fprintf(stderr, "(%ld) clean_write: ERROR with write, caught EPIPE\n", pthread_self());
            fcntl(fd, F_SETFL, O_NONBLOCK); // Set socket to non-blocking
            return 0;
        } else if(errno == 104){
            fprintf(stderr, "(%ld) clean_write: ERROR with write, caught \"Connnection reset by peer\"\n", pthread_self());
            fcntl(fd, F_SETFL, O_NONBLOCK); // Set socket to non-blocking
            return 0;
        } else {
            fprintf(stderr, "(%ld) clean_write: ERROR with write, errno of %d\n", pthread_self(), errno);
            perror("");
            fcntl(fd, F_SETFL, O_NONBLOCK); // Set socket to non-blocking
            return -1;
        }
    }
    fcntl(fd, F_SETFL, O_NONBLOCK); // Set socket to non-blocking
    return bytes;
}

int send_data(int conn_fd, char *header, FILE *file, uint32_t file_sz){
    int rv = 0;
    size_t bytes_left, bytes_written;

    if(header != NULL){
        // write the header
        bytes_left = (size_t) strlen(header); // bytes left in the header to send
        bytes_written = 0; // bytes written from the header
        while(bytes_left > 0){
            size_t bytes_no = bytes_left > MAX_WRITE_SZ ? MAX_WRITE_SZ : bytes_left; // bytes to write this call
            rv = clean_write(conn_fd, header + (bytes_written), bytes_no);
            if(rv == 0){
                return 0;
            } else if (rv < 0){
                fprintf(stderr, "(%ld) send_response: ERROR with clean_write\n", pthread_self());
                return rv;
            }
            bytes_left -= bytes_no;
            bytes_written += bytes_no;
        }
    }

    // Check if file exists
    if(file == NULL) return 0;

    // write the file
    char file_buf[MAX_WRITE_SZ];
    bytes_left = (size_t) file_sz; // bytes left in the file to send
    bytes_written = (size_t) 0; // bytes written from the file

    // For the rest of the data in the file (or the entire file for GET requests)
    // read the file in chunks in to a file buffer and then write that file buffer
    // into the socket repeatedly until EoF
    while(bytes_left > 0){
        size_t bytes_no = bytes_left > MAX_WRITE_SZ ? MAX_WRITE_SZ : bytes_left; // bytes to write this call
        if(fread((void *) file_buf, (size_t) 1, bytes_no, file) == 0){
            fprintf(stderr, "(%ld) send_response: ERROR with fread (file_sz %u, bytes_no %lu)\n", pthread_self(), file_sz, bytes_no);
            return -1;
        }
        rv = clean_write(conn_fd, file_buf, bytes_no);
        if(rv == 0){
            return 0;
        } else if (rv < 0){
            fprintf(stderr, "(%ld) send_response: ERROR with clean_write\n", pthread_self());
            return rv;
        }
        bytes_left -= bytes_no;
        bytes_written += bytes_no;
    }

    return 0;
}

uint32_t get_file_size(FILE * file){
    uint32_t file_sz;
    // Calculate file size in bytes
    if(fseek(file, 0, SEEK_END) < 0){
        printf("(%ld) get_file_size: ERROR with fseek\n", pthread_self());
        return -1;
    }
    if((file_sz = ftell(file)) < 0){
        printf("(%ld) get_file_size: ERROR ftell\n", pthread_self());
        return -1;
    }
    if(PRINTS & PRINT_FILE_SIZE) printf("(%ld) File size : %u bytes\n", pthread_self(), file_sz);
    // Return to start of file
    if(fseek(file, 0, SEEK_SET) < 0){
        printf("(%ld) get_file_size: ERROR with fseek\n", pthread_self());
        return -1;
    }
    return file_sz;
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

// Place size of header from `buf` into `n`
int get_header_size(char *buf, uintptr_t *n){
    char *loc;
    if ((loc = strstr(buf, "\r\n\r\n")) == NULL){
        fprintf(stderr, "(%ld) get_content_length: ERROR finding \"Content-Length: \" in buffer\n", pthread_self());
        return -1;
    }
    loc += 4; //Update offset to start of data
    *n = (uintptr_t)loc - (uintptr_t)buf;
    return 0;
}

// Extract 'Content-Length: ' header value from 'buf', reading at most 'n' bytes in 'buf'
int get_content_length(char *buf, char *ret){
    memset(ret, 0, MAX_CONTENT_SZ);
    char *loc1, *loc2;
    if ((loc1 = strstr(buf, "\r\nContent-Length: ")) == NULL){
        if(PRINTS) fprintf(stderr, "(%ld) get_content_length: ERROR finding \"Content-Length: \" in buffer\n", pthread_self());
        return -1;
    }
    loc1 += 18; //Update offset to value of 'Content-Length: '
    if ((loc2 = strchr(loc1, '\r')) == NULL){
        fprintf(stderr, "(%ld) get_content_length: ERROR finding end of \"Content-Length: \" line\n", pthread_self());
        return -1;
    }
    size_t content_sz = (size_t)loc2 - (size_t)loc1;
    if (content_sz > MAX_CONTENT_SZ){
        fprintf(stderr, "(%ld) get_content_length: ERROR, MAX_CONTENT_SZ exceeded\n", pthread_self());
        return -1;
    }
    strncpy(ret, loc1, content_sz);
    return 0;
}

int extract_req_fields(char *req_buf, char *req_meth, char *req_uri, char *req_ver, char *req_host, char *req_port){
    // Expect format of header:
    /*
     * <REQ_METH> <REQ_URI><:REQ_PORT (optional)> <REQ_VER>\r\n
     * <REQ_HOST><:REQ_PORT (oprtional)>
     * /r/n/r/n
     */
    char *loc; // Location of character got from strchr
    int use_uri_host = 0; // If Host: isn't in the header, extract it from the URI

    // Get the positions of field delimeters
    if ((loc = strchr(req_buf, ' ')) == NULL){
        fprintf(stderr, "(%ld) extract_req_fields: ERROR finding first space in request\n", pthread_self());
        return -1;
    }
    uintptr_t space_1 = loc - req_buf;

    if ((loc = strchr(req_buf + space_1 + 1, ' ')) == NULL){
        fprintf(stderr, "(%ld) extract_req_fields: ERROR finding second space in request\n", pthread_self());
        return -1;
    }
    uintptr_t space_2 = loc - req_buf;

    if ((loc = strchr(req_buf, '\r')) == NULL){
        fprintf(stderr, "(%ld) extract_req_fields: ERROR finding end of first line in request\n", pthread_self());
        return -1;
    }
    uintptr_t line_end_1 = loc - req_buf;

    // Ensure that the ordering of the first two spaces and the line end are what we expect
    if(space_1 >= space_2 || space_1 >= line_end_1 || space_2 >= line_end_1){
        fprintf(stderr, "(%ld) extract_req_fields: ERROR finding the end of the first line in request\n", pthread_self());
        return -1;
    }

    // Check for 'Host: ' line
    if ((loc = strstr(req_buf, "\r\nHost:")) == NULL){
        use_uri_host = 1; // We know Host: exists so we'll use that to extract our data
    }

    // General pointers to extract Host info
    uintptr_t host_line = 0;
    uintptr_t host_start = 0;
    uintptr_t host_end = 0;

    if(!use_uri_host){
        // We WON'T be using the URI to get host information, we'll use the Host: line
        host_line = loc - req_buf; // Start search from '\r\nHost: '
    
        // We are using URI to get the host
        // First try for '://', then try for 'www', then just use the first character after the first space
        if ((loc = strstr(req_buf + host_line, "://")) == NULL){
            if ((loc = strstr(req_buf + host_line, "www")) == NULL){
                // Neither :// nor www exists, get first space
                if ((loc = strchr(req_buf + host_line, ' ')) == NULL){
                    fprintf(stderr, "(%ld) extract_req_fields: ERROR finding first space in \"Host:\" line\n", pthread_self());
                    return -1;
                }
                host_start = loc - req_buf;
            } else {
                // www exists
                host_start = loc - req_buf - 1; // -1 to so that we are just before the host
                                                // moves start (x) from '(w)ww.host' to '(x)www.host'
            }
        } else {
            // :// exists
            host_start = loc - req_buf + 2; // +2 to so that we are just before the host
                                            // moves start (x) from '(:)//host' to ':/(/)host'
        }

        // Find end of line
        if ((loc = strchr(req_buf + host_start + 1, '\r')) == NULL){
            fprintf(stderr, "(%ld) extract_req_fields: ERROR finding second end-line\n", pthread_self());
            return -1;
        }
        // Find end of line
        if ((loc = strchr(req_buf + host_start + 1, '\r')) == NULL){
            fprintf(stderr, "(%ld) extract_req_fields: ERROR finding second end-line\n", pthread_self());
            return -1;
        }
        host_end = loc - req_buf;
        // Find '/'
        if ((loc = strchr(req_buf + host_start + 1, '/')) == NULL){
        }
        // See which is smaller, end of the line or next '/'
        host_end = loc - req_buf < host_end ? loc - req_buf : host_end;
    } else {
        // We are using URI to get the host
        // First try for '://', then try for 'www', then just use the first character after the first space
        if ((loc = strstr(req_buf, "://")) == NULL){
            if ((loc = strstr(req_buf, "www")) == NULL){
                host_start = space_1;
            } else {
                // www exists
                host_start = loc - req_buf - 1; // -1 to so that we are just before the host
                                                // moves start (x) from '(w)ww.host' to '(x)www.host'
            }
        } else {
            // :// exists
            host_start = loc - req_buf + 2; // +2 to so that we are just before the host
                                            // moves start (x) from '(:)//host' to ':/(/)host'
        }
        if ((loc = strchr(req_buf + host_start + 1, '/')) == NULL){
        } else {
            // See which is smaller, next space or next '/'
            host_end = loc - req_buf < space_2 ? loc - req_buf : space_2;
        }
    }
    uintptr_t colon_1;

    if ((loc = strchr(req_buf + host_start + 1, ':')) == NULL){
        colon_1 = UINTPTR_MAX;
    } else {
        colon_1 = loc - req_buf;
    }

    // Check if there is a port number in the `Host` part of the HTTP header
    if(colon_1 < host_end){
        if(host_end - colon_1 - 1 >= MAX_PORT)
            fprintf(stderr, "(%ld) extract_req_fields: ERROR Port string too long\n", pthread_self());
        strncpy(req_port, req_buf + colon_1 + 1, host_end - colon_1 - 1);
        if(colon_1 - host_start - 1 >= MAX_HOST)
            fprintf(stderr, "(%ld) extract_req_fields: ERROR Host string too long\n", pthread_self());
        strncpy(req_host, req_buf + host_start + 1, colon_1 - host_start - 1);
    } else {
        strcpy(req_port, "80");
        strncpy(req_host, req_buf + host_start + 1, host_end - host_start - 1);
    }

    // Fill in the request fields
    if(space_1 >= MAX_METH)
        fprintf(stderr, "(%ld) extract_req_fields: ERROR method string too long\n", pthread_self());
    if(space_2 - space_1 - 1 >= MAX_URI)
        fprintf(stderr, "(%ld) extract_req_fields: ERROR method string too long\n", pthread_self());
    if(line_end_1 - space_2 - space_1 - 1 >= MAX_VER)
        fprintf(stderr, "(%ld) extract_req_fields: ERROR method string too long\n", pthread_self());
    strncpy(req_meth, req_buf, space_1);
    strncpy(req_uri, req_buf + space_1 + 1, space_2 - space_1 - 1);
    strncpy(req_ver, req_buf + space_2 + 1, line_end_1 - space_2 - 1);
    return 0;
}

ssize_t read_conn_fd(int *conn_fd, char *buf, int n){
    ssize_t n_read;  // # of bytes read from conn_fd stored in n_read
    struct timespec curr_time, req_time; // current time (checked in loop) and request time (initialized once)
    clock_gettime(CLOCK_MONOTONIC, &req_time); // Set request time
    
    while((n_read = read(*conn_fd, buf, n)) == -1){
        if(errno == EAGAIN || errno == EWOULDBLOCK){
            clock_gettime(CLOCK_MONOTONIC, &curr_time);
            // Check timeout
            if(req_time.tv_sec + ((time_t) CONN_TIMEOUT) <= curr_time.tv_sec && req_time.tv_nsec <= curr_time.tv_nsec){
                // Return -2 since we timed out
                if(PRINTS & PRINT_DIVIDERS & PRINT_TIMEOUT) printf("------------------------\n");
                if(PRINTS & PRINT_TIMEOUT) printf("(%ld) read_conn_fd: timed out after %ld.%.9ld\n", pthread_self(), curr_time.tv_sec - req_time.tv_sec, curr_time.tv_nsec - req_time.tv_nsec);
                if(PRINTS & PRINT_DIVIDERS & PRINT_TIMEOUT) printf("------------------------\n");
                return -2;
            }
            continue;
        } else if(errno == EPIPE){
            // This means the connection was closed by the other end
            fprintf(stderr, "(%ld) read_conn_fd: ERROR with read, caught EPIPE\n", pthread_self());
            return 0;
        } else if(errno == 104){
            // This also means the connection was closed by the other end
            fprintf(stderr, "(%ld) read_conn_fd: ERROR with read, caught \"Connnection reset by peer\"\n", pthread_self());
            return 0;
        } else {
            // This is an actual error
            fprintf(stderr, "(%ld) read_conn_fd: ERROR reading from conn_fd\n", pthread_self());
            perror("");
            return -1;
        }
    }
    // Return the number of bytes read
    return n_read;
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

// Print the names of the items in the linked list
void print_ll(){
    pthread_mutex_lock(&ll_lock);
    if(head == NULL)
        return;

    node *tmp = head;

    // See if the next value of tmp is ptr, then delete it and update the LL
    for(; tmp->next != NULL; tmp = tmp->next){
        printf("%s->", tmp->md5);
    }

    printf("%s\n", tmp->md5);

    pthread_mutex_unlock(&ll_lock);
}

// Create an empty node, returns the memory value of the malloc'd pointer
node * create_node(){
    unsigned long node_sz = sizeof(node);
    node *ptr = (node *) malloc(node_sz);
    memset(ptr, 0, node_sz);
    return ptr;
}

// Given a node *, update its time to the current time
void update_time(node *ptr){
    clock_gettime(CLOCK_MONOTONIC, &(ptr->cache_time));
}

// Given a node *, update its name (md5)
void update_name(node *ptr, char *name){
    memset(ptr->md5, 0, 39);
    strncpy(ptr->md5, name, 38);
}

// Add node to head of linked list
// (more recently created files are more likely to be accessed)
void add_node(node *ptr){
    pthread_mutex_lock(&ll_lock);
    if(head == NULL){
        // LL doesn't exist
        head = ptr;
    } else {
        // LL does exist
        ptr->next = head;
        head = ptr;
    }
    pthread_mutex_unlock(&ll_lock);
}

// Given a node *, delete it from the linked list freeing memory
void delete_node(node *ptr){
    pthread_mutex_lock(&ll_lock);

    // See if there's anything in the LL
    if(head == NULL){
        pthread_mutex_unlock(&ll_lock);
        return;
    }

    // See if the node is head
    if(head == ptr){
        head = ptr->next;
        free(ptr);
        pthread_mutex_unlock(&ll_lock);
        return;
    }

    // See if the next value of tmp is ptr, then delete it and update the LL
    for(node *tmp = head; tmp->next != NULL; tmp = tmp->next){
        if(tmp->next == ptr){
            tmp->next = tmp->next->next;
            free(ptr);
            pthread_mutex_unlock(&ll_lock);
            return;
        }
    }

    // Node isn't in LL
    pthread_mutex_unlock(&ll_lock);
}

node * check_node(char *name){
    pthread_mutex_lock(&ll_lock);

    // See if there's anything in the LL
    if(head == NULL){
        pthread_mutex_unlock(&ll_lock);
        return NULL;
    }

    // Check LL for matching name
    for(node *tmp = head; tmp != NULL; tmp = tmp->next){
        if(strncmp(tmp->md5, name, 38) == 0){
            pthread_mutex_unlock(&ll_lock);
            return tmp;
        }
    }

    // Node isn't in LL
    pthread_mutex_unlock(&ll_lock);
    return NULL;
}

int check_timeout(node *ptr){
    struct timespec curr_time;
    clock_gettime(CLOCK_MONOTONIC, &curr_time);
    // TODO: there may be a better way to do this... this works though
    //       for some reason I couldn't get the same method in read_conn_fd to work
    if(difftime(curr_time.tv_sec, ptr->cache_time.tv_sec) >= (double) cache_timeout){
        return 1;
    }
    return 0;
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

    FILE *cmd_response;

    pthread_mutex_lock(&resolved_lock);
    
    if((cmd_response = popen(cmd, "r")) == NULL){
        fprintf(stderr, "get_md5: ERROR calling \"popen\" with command \"%s\"\n", cmd);
        pthread_mutex_unlock(&resolved_lock);
        return -1;
    }

    if(pclose(cmd_response)){
        fprintf(stderr, "get_md5: ERROR with pclose\n");
    }

    pthread_mutex_unlock(&resolved_lock);
    return 0;
}

// Given a host_name, save the host_ip to host_ip of size MAX_URI
int get_ip(char *host_name, char *host_ip){
    FILE *file;
    char buf[MAX_URI];
    memset(buf, 0, MAX_URI);

    pthread_mutex_lock(&resolved_lock);

    if((file = fopen("resolved.txt", "rb")) == NULL){
        fprintf(stderr, "File `resolved.txt` cannot be opened\n");
        pthread_mutex_unlock(&resolved_lock);
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
            pthread_mutex_unlock(&resolved_lock);
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
                pthread_mutex_unlock(&resolved_lock);
                return -1;
            }
            memset(name1, 0, MAX_URI); memset(name2, 0, MAX_URI);
            strncpy(name1, start, space - start); // 123 456\n
            strncpy(name2, space+1, end - (space + 1));

            // Check if we have a hit
            if(strcmp(name1, host_name) == 0){
                strncpy(host_ip, name2, MAX_URI - 1);
                fclose(file);
                pthread_mutex_unlock(&resolved_lock);
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
            pthread_mutex_unlock(&resolved_lock);
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
    pthread_mutex_unlock(&resolved_lock);
    return -1;
}
