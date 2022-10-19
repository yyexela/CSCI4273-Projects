/* 
 * TODO:
 * - make something to parse through directory of tempory stored pieces, check LL for completeness, and call cat from shell
 * 
 * DOING:
 * 
 * DONE:
 * - Parse query (as a part of calling list) will fetch pieces fo files from servers when it encounters them in the temp file
 *   while get is being called
 * - Most of the infrastructure for receiving files
 * - Update recv_file to store stuff in the right place
 */

#include <stdio.h>      // For print statements
#include <unistd.h>     // For File IO
#include <string.h>     // For string manipulation
#include <signal.h>     // For signal handling
#include <stdlib.h>     // For exit() function
#include "client.h"     // For defines and structs
#include <sys/types.h>  // For TCP/Networking
//#include <fcntl.h>      // - Used for non-blocking sockets
#include <sys/socket.h> 
#include <netdb.h>
#include <time.h>       // Used for timing out when connecting to servers   
#include <errno.h>      // Used for catching errors
#include <sys/stat.h>   // Used for C mkdir function

int main(int argc, char *argv[]){
    Server servers[4];     // Store all 4 server infos in an array of structs
    char line[MAX_LINE];   // Where we store user input
    line[0] = '\0';        // Remove warning when compiling
    
    if(init_cmdline(argc, argv, servers) < 0){
        fprintf(stderr, "main: Error with init_cmdline\n");
        return 0;
    }

    if(signal(SIGINT, sigint_handler) == SIG_ERR){
        fprintf(stderr, "main: Error with signal\n");
        return 0;
    }

    do{
        print_menu();
        get_message(line, MAX_LINE);
        if(PRINTS & PRINT_USER_INPUT) fprintf(stdout, "User input: first char \"(%u)\" \"%s\"\n", line[0], line);

        if(strncmp(line, "list", 4) == 0){
            run_list_get(servers, 0, line);
        } else if (strncmp(line, "get ", 4) == 0){
            run_list_get(servers, 1, line);
        } else if (strncmp(line, "put ", 4) == 0){
            run_put(servers, line);
        }

    } while(line[0] != 0);

    printf("Exiting safely\n");
}

void run_put(Server * servers, char * cmd){
    char * filename = cmd + 4; // Cut off 'put ', leaving filename
    FILE * file;
    int filesize;
    int offset;
    char md5_str[33];
    int part_sizes[4];
    unsigned long long md5_val;

    // Take off " ./" at the beginning by removing it if it exists for the filename
    if(strncmp(filename, "./", 2) == 0){
        filename = filename + 2;
    }

    // Open file
    if((file = fopen(filename, "rb")) == NULL){
        fprintf(stderr, "run_put: ERROR opening file \"%s\"\n", filename);
        return;
    }

    // Get file size
    if((filesize = (int) get_file_size(file)) < 0){
        fprintf(stderr, "run_put: ERROR with getting file size for \"%s\"\n", filename);
        fclose(file);
        return;
    }

    // Calculate part sizes
    if(get_part_sizes(part_sizes, filesize) < 0){
        fprintf(stderr, "run_put: ERROR with calculating part sizes for get_part_sizes\n");
        fclose(file);
        return;
    }

    // Calculate md5 value of the file
    if(get_md5(filename, md5_str) < 0){
        fprintf(stderr, "run_put: Error in md5_str for \"%s\"\n", filename);
        fclose(file);
    }

    // Calculate the md5 modulo 4
    md5_val = strtoull(md5_str + 16, NULL, 16);
    offset = md5_val % 4;
    if(PRINTS & PRINT_OFFSET) printf("Offset: %d\n", offset);

    // Send the parts of the files to the servers
    for(int i = 0; i < 4; i++){
        for(int j = 0; j < 2; j++){
            int server_no = (i + offset) % 4;
            int segment_no = (i + j) % 4;
            if(seek_to(file, part_sizes, segment_no) < 0){
                fprintf(stderr, "Failed seeking to beginning of segment %d\n", segment_no + 1);
                fclose(file);
                return;
            }
            if(send_part(servers, filename, part_sizes, segment_no, server_no, file) < 0){
                fprintf(stderr, "Failed sending data to server %d\n", server_no + 1);
                fclose(file);
                return;
            }
        }
    }
    fclose(file);
}

int send_part(Server * servers, char *filename, int part_sizes[4], int part_no, int server_no, FILE *file){
    int rv = 0, server_fd;
    struct addrinfo *serverinfo = NULL;

    if(tcp_timeout_connect(&server_fd, serverinfo, (servers[server_no]).ip, servers[server_no].port) < 0)
        return 0; // We timed out, pretend everything is good

    // Construct metadata in header
    char header[MAX_WRITE_SZ];
    memset(header, 0, MAX_WRITE_SZ);
    sprintf(header, "put %s %d %d ", filename, part_sizes[part_no], part_no + 1);
    if(PRINTS & PRINT_SEND_INFO) printf("hdr %s\n", header);

    rv = send_string(header, server_fd);
    if(rv == 1){
        fprintf(stderr, "send_part: Caught error with send_string, returning 0\n");
        return 0;
    } else if(rv < 0){
        fprintf(stderr, "send_part: Error with send_string\n");
        return rv;
    }

    // Check if file exists
    if(file == NULL){
        fprintf(stderr, "send_part: ERROR with file\n");
        return 0;
    }

    // write the file
    rv = send_file(file, server_fd, (size_t) part_sizes[part_no]);
    if(rv == 1){
        return 0;
    } else if(rv < 0){
        return rv;
    }

    close(server_fd);
    freeaddrinfo(serverinfo);

    return 0;
}

int seek_to(FILE * file, int part_sizes[4], int part_no){
    long offset = 0;
    for(int i = 0; i < part_no; i++){
        offset += part_sizes[i];
    }
    if(fseek(file, offset, SEEK_SET) < 0){
        fprintf(stderr, "seek_to: Error with fseek\n");
        return -1;
    }
    return 0;
}

int get_part_sizes(int sizes[4], int filesize){
    int equal = filesize / 4;
    int rem = filesize % 4;

    // Add main chunk sizes
    for(int i = 0; i < 4; i++){
        sizes[i] = equal;
    }

    // Add left-overs
    for(int i = 0; i < rem; i++){
        sizes[i] += 1;
    }

    // Check
    if(sizes[0] + sizes[1] + sizes[2] + sizes[3] != filesize){
        fprintf(stderr, "part_sizes: Incorrectly split sizes\n");
        return -1;
    }

    return 0;
}

int get_md5(char *filename, char *ret){
    // Set return value to empty string
    memset(ret, '\0', 33);

    //printf("(%ld) \n", pthread_self());
    if(PRINTS & PRINT_MD5) printf("get_md5: Converting \"%s\"\n", filename);

    // Create cmd string
    size_t filename_len = strlen(filename);
    char *cmd = malloc(filename_len + 17);
    strcpy(cmd, "md5sum -b \'");
    strcpy(cmd + strlen("md5sum -b \'"), filename);
    strcpy(cmd + strlen("md5sum -b \'") + filename_len, "\'");
    
    if(PRINTS & PRINT_MD5) printf("get_md5: Calling %s\n", cmd);

    FILE *cmd_response;
    if((cmd_response = popen(cmd, "r")) == NULL){
        fprintf(stderr, "get_md5: ERROR calling \"popen\" with command \"%s\"\n", cmd);
        return -1;
    }

    // Read md5 result into buffer `ret`
    size_t pos = 0;
    size_t bytes_read = 0;
    size_t bytes_left = 32;
    while(pos != 32){
        if((bytes_read = fread(ret + pos, 1, bytes_left, cmd_response)) < 0){
            fprintf(stderr, "get_md5: ERROR calling \"fread\"\n");
            return -1;
        }
        pos += bytes_read;
    }

    if(strlen(ret) > 32){
        fprintf(stderr, "get_md5: ERROR returned more than 32 bytes\n");
        return -1;
    }

    if(pclose(cmd_response)){
        fprintf(stderr, "get_md5: ERROR with pclose\n");
    }

    if(PRINTS & PRINT_MD5) printf("get_md5: Returning %s\n", ret);

    return 0;
}

void run_list_get(Server * servers, int is_get, char * cmd){
    char * filename = cmd + 4; // Cut off 'get ', leaving filename (only to be used while is_get)
    FILE * file;

    // Open tmp file for storing results
    file = fopen("tmp", "wb+");
    if(file == NULL){
        fprintf(stderr, "run_list: Error in opening file for \"tmp\"\n");
        return;
    }

    // Ask each server for its files, writing to `tmp`
    for(int i = 0; i < 4; i++){
        if(get_query(servers, i, file) < 0){
            fprintf(stderr, "run_list: Failed getting query from server %d\n", i + 1);
            return;
        }
    }

    // Seek back to start of file
    if(fseek(file, 0, SEEK_SET) < 0){
        fprintf(stderr, "run_list: Error with fseek\n");
        return;
    }

    // Parse query, printing server file status
    parse_query(file, is_get, filename, servers);

    // Remove and close the file
    fclose(file);
    remove("tmp");
}

int get_query(Server * servers, int server_no, FILE * file){
    int server_fd, rv;
    struct addrinfo *serverinfo = NULL;

    if(tcp_timeout_connect(&server_fd, serverinfo, (servers[server_no]).ip, servers[server_no].port) < 0)
        return 0; // We timed out or couldn't connect, nothing else to do

    // Construct the command we are sending to the servers
    char cmd[MAX_WRITE_SZ];
    memset(cmd, 0, MAX_WRITE_SZ);
    sprintf(cmd, "list");

    // Send the cmd
    rv = send_string(cmd, server_fd);
    if(rv == 1){
        fprintf(stderr, "query: Caught error with send_string, returning 0\n");
        close(server_fd);
        freeaddrinfo(serverinfo);
        return 0;
    } else if(rv < 0){
        fprintf(stderr, "query: Error with send_string\n");
        close(server_fd);
        freeaddrinfo(serverinfo);
        return rv;
    }

    // Read response into file
    if(read_into_file(server_fd, serverinfo, file) != 0){
        fprintf(stderr, "query: Error with read_into_file\n");
        close(server_fd);
        freeaddrinfo(serverinfo);
        return -1;
    }

    // Done with server connection
    close(server_fd);
    freeaddrinfo(serverinfo);
    return 0;
}

int read_into_file(int server_fd, struct addrinfo *serverinfo, FILE * file){
    // Get back data, reading until socket closes
    char buf[MAX_WRITE_SZ];
    memset(buf, 0, MAX_WRITE_SZ);
    int total_read = 0, bytes_read, bytes_written;
    while((bytes_read = read(server_fd, buf, MAX_READ_SZ - 1)) > 0){
        if((bytes_written = fwrite(buf + total_read, 1, (size_t) bytes_read, file)) < 0){
            fprintf(stderr, "read_into_file: Error with fwrite\n");
            return -1;
        }
        total_read += bytes_read;
    }
    return 0;
}

void parse_query(FILE * file, int is_get, char * filename, Server * servers){
    char line[MAX_LINE];
    int rv = 0;
    memset(line, 0, MAX_LINE);

    // Make temporary directory for pieces if get call
    if(is_get){
        if(mkdir("get_tmp", S_IRWXU) != 0 && errno != EEXIST){
            fprintf(stderr, "parse_query: Error in creating directory for \"get_tmp\"\n");
            return;
        }
    }

    // For each line, add it to the linked list
    node * head = NULL;
    char * loc = NULL;
    char * loc2 = NULL;
    while(fgets(line, MAX_LINE, file)){
        // Overwrite the newline/EOF at the end of each line with null char so we
        // just have the data
        if(strlen(line) > 0)
            line[strlen(line)-1] = '\0';

        // Update linked list
        loc = strchr(line, ' ');
        loc2 = strchr(loc+1, ' ');
        if(loc == NULL || loc2 == NULL){
            memset(line, 0, MAX_LINE);
            continue;
        } else {
            loc[0] = '\0'; // Split string into 3
            loc2[0] = '\0'; // <file_name> <parts> <server_directory>
            node * curr;
            if((curr = check_node(head, line)) == NULL){
                // Node doesn't exist in linked list
                curr = create_node();
                update_name(curr, line);
                add_node(&head, curr);
            }
            // Node exists in linked list by now, update part values
            for(int i = 0; i < 4; i++){
                if(strchr(loc + 1, '1' + i)){
                    (curr->part)[i] = 1;
                    if(is_get && strcmp(line, filename) == 0) fetch_piece(servers, filename, i, loc2+1);
                }
            }
        }
    }

    // Go through and print the list (if !is_get aka list)
    if(!is_get) print_ll(head);

    // TODO: deal with temp directory for retrieved file and then delete the directory(if is_get)
    if(is_get){
        if((rv = compile_get(servers, filename)) == 1){
            printf("File is incomplete\n");
        } else if (rv == 0){
            if(PRINTS & PRINT_GET_FILES)printf("File: \"%s\" successfully retrieved\n", filename);
        }

        FILE *cmd_response;
        if((cmd_response = popen("rm -r get_tmp", "r")) == NULL){
            fprintf(stderr, "parse_query: ERROR calling \"popen\" with command \"%s\"\n", "rm -r tmp");
            return;
        }
        if(pclose(cmd_response)){
            fprintf(stderr, "parse_query: ERROR with pclose\n");
        }
    }

    // Free the linked list
    delete_ll(head);
}

int fetch_piece(Server * servers, char * filename, int part_no, char * server_dir){
    int server_no;
    int rv = 0;
    int server_fd;
    struct addrinfo *serverinfo = NULL;

    // Find the server in our array based on its directory name
    for(int i  = 0; i < 4; ++i){
        if(strcmp(servers[i].dir, server_dir) == 0) server_no = i;
    }

    if(tcp_timeout_connect(&server_fd, serverinfo, (servers[server_no]).ip, servers[server_no].port) < 0)
        return 0; // We timed out, pretend everything is good

    // Construct metadata in header
    char header[MAX_WRITE_SZ];
    memset(header, 0, MAX_WRITE_SZ);
    sprintf(header, "get %s %d ", filename, part_no + 1);
    if(PRINTS & PRINT_SEND_INFO) printf("hdr %s\n", header);

    rv = send_string(header, server_fd);
    if(rv == 1){
        fprintf(stderr, "fetch_piece: Caught error with send_string, returning 0\n");
        return 0;
    } else if(rv < 0){
        fprintf(stderr, "fetch_piece: Error with send_string\n");
        return rv;
    }

    if(recv_get_response(server_fd, server_dir, filename)){
        fprintf(stderr, "fetch_piece: Caught error receiving response from server, returning 0\n");
        freeaddrinfo(serverinfo);
        return 0;
    }

    close(server_fd);
    freeaddrinfo(serverinfo);

    return 0;
}

int compile_get(Server * servers, char * filename){
    int cmd_index = 0;
    int found_pieces = 0;
    char cmd_buf[MAX_LINE];
    char path_buf[MAX_LINE];

    memset(cmd_buf, 0, MAX_LINE);
    strcpy(cmd_buf + cmd_index, "cat ");
    cmd_index += strlen("cat ");

    for(int piece_no = 0; piece_no < 4; piece_no++){
        for(int server_no = 0; server_no < 4; server_no++){
            make_path(path_buf, servers, server_no, piece_no+1);

            if(access(path_buf, F_OK) == 0){
                strcpy(cmd_buf + cmd_index, path_buf);
                cmd_index += strlen(path_buf);
                strcpy(cmd_buf + cmd_index, " ");
                cmd_index += strlen(" ");
                found_pieces++;
                break;
            }
        }

        if(found_pieces < piece_no + 1){
            return 1;
        }
    }

    strcpy(cmd_buf + cmd_index, "> ");
    cmd_index += strlen("> ");
    strcpy(cmd_buf + cmd_index, filename);
    cmd_index += strlen(filename);

    FILE *cmd_response;
    if((cmd_response = popen(cmd_buf, "r")) == NULL){
        fprintf(stderr, "compile_get: ERROR calling \"popen\" with command \"%s\"\n", cmd_buf);
        return -1;
    }
    if(pclose(cmd_response)){
        fprintf(stderr, "compile_get: ERROR with pclose\n");
    }

    return 0;
}

void make_path(char * path_buf, Server * servers, int server_no, int piece_no){
    memset(path_buf, 0, MAX_LINE);
    int path_index = 0;

    strcpy(path_buf + path_index, "get_tmp/");
    path_index += strlen("get_tmp/");

    strcpy(path_buf + path_index, servers[server_no].dir);
    path_index += strlen(servers[server_no].dir);

    strcpy(path_buf + path_index, "/");
    path_index += strlen("/");

    sprintf(path_buf + path_index, "%d", piece_no);
    path_index += 1;
}

int recv_get_response(int server_fd, char * server_dir, char * filename){
    char buf[MAX_READ_SZ];
    memset(buf, 0, MAX_READ_SZ);
    int bytes_read;

    bytes_read = read(server_fd, buf, MAX_READ_SZ - 1); 
    if(bytes_read == -1){
        fprintf(stderr, "thread_main: Error calling read\n");
        close(server_fd);
        return 1;
    }

    // Check type
    if(strncmp(buf, "put", 3) == 0){
        if(recv_file(server_fd, buf, server_dir, bytes_read) < 0){
            fprintf(stderr, "thread_main: Error in calling receiving file\n");
            close(server_fd);
            return 1;
        }
    }

    close(server_fd);
    return 0;
}

void delete_ll(node * head){
    while(head != NULL){
        delete_node(&head, head);
    }
}

void print_ll(node *head){
    // Check LL for matching name
    for(node *tmp = head; tmp != NULL; tmp = tmp->next){
        int complete = 1;
        for(int i = 0; i < 4; i++){
            if(tmp->part[i] == 0) complete = 0;
        }
        if(complete){
            fprintf(stdout, "%s\n", tmp->filename);
        } else {
            fprintf(stdout, "%s [incomplete]\n", tmp->filename);
        }
    }
}

// Given a node *, update its name (md5)
void update_name(node *ptr, char *name){
    memset(ptr->filename, 0, MAX_LINE);
    strncpy(ptr->filename, name, MAX_LINE - 1);
}

// Create an empty node, returns the memory value of the malloc'd pointer
node * create_node(){
    unsigned long node_sz = sizeof(node);
    node *ptr = (node *) malloc(node_sz);
    memset(ptr, 0, node_sz);
    for(int i = 0; i < 4; i++){
        ptr->part[i] = 0;
    }
    return ptr;
}

// Add node to head of linked list
// (more recently created files are more likely to be accessed)
void add_node(node **head, node *ptr){
    if(*head == NULL){
        // LL doesn't exist
        *head = ptr;
    } else {
        // LL does exist
        ptr->next = *head;
        *head = ptr;
    }
}

// Given a node *, delete it from the linked list freeing memory
void delete_node(node **head, node *ptr){
    // See if there's anything in the LL
    if(*head == NULL){
        return;
    }

    // See if the node is head
    if(*head == ptr){
        *head = ptr->next;
        free(ptr);
        return;
    }

    // See if the next value of tmp is ptr, then delete it and update the LL
    for(node *tmp = *head; tmp->next != NULL; tmp = tmp->next){
        if(tmp->next == ptr){
            tmp->next = tmp->next->next;
            free(ptr);
            return;
        }
    }
}

node * check_node(node * head, char *filename){
    // See if there's anything in the LL
    if(head == NULL){
        return NULL;
    }

    // Check LL for matching name
    for(node *tmp = head; tmp != NULL; tmp = tmp->next){
        if(strncmp(tmp->filename, filename, MAX_LINE) == 0){
            return tmp;
        }
    }

    // Node isn't in LL
    return NULL;
}

int tcp_timeout_connect(int *server_fd_ptr, struct addrinfo *serverinfo, char *server_ip, char *server_port){
    struct timespec curr_time, start_time;
    double time = 0;

    // Initiate TCP connection with the server
    // Try connecting for 1 second
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    clock_gettime(CLOCK_MONOTONIC, &curr_time);
    while((time = difftime(curr_time.tv_sec, start_time.tv_sec)) < (double) 1){
        clock_gettime(CLOCK_MONOTONIC, &curr_time);
        if(init_server_tcp(server_fd_ptr, serverinfo, server_ip, server_port) < 0){
            // Try again
            continue;
        } else {
            //Connected
            break;
        }
    }

    // Check if we timed out
    if(time >= 1){
        if(PRINTS & PRINT_SERVER_CONN)
            fprintf(stderr, "tcp_timeout_connect: Error initializing TCP connection to server \"%s:%s\"\n", 
                server_ip, server_port);
        return -1;
    } else {
        if(PRINTS & PRINT_SERVER_CONN)
            fprintf(stdout, "Successfully connected to server \"%s:%s\"\n", 
                server_ip, server_port);
    }

    return 0;
}

int close_connections(int * server_fd, struct addrinfo ** serverinfo){
    for(int i = 0; i < 4; i++){
        close(server_fd[i]);
        freeaddrinfo(serverinfo[i]);
    }
    return 0;
}

int init_server_tcp(int *server_fd, struct addrinfo *serverinfo, 
                    char *req_host, char *req_port){
    int status;
    struct addrinfo *p;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;      // IPV4 or IPV6
    hints.ai_socktype = SOCK_STREAM;  // TCP

    if((status = getaddrinfo(req_host, req_port, &hints, &serverinfo)) != 0){
        if(PRINTS & PRINT_CONN_ERR) fprintf(stderr, "init_server_tcp: Error in getaddrinfo\n");
        return -1;
    }

    for(p = serverinfo; p != NULL; p = p->ai_next){
        if((*server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            if(PRINTS & PRINT_CONN_ERR)fprintf(stderr, "init_server_tcp: Error in socket\n");
            continue;
        }

        if((status = connect(*server_fd, p->ai_addr, p->ai_addrlen)) == -1){
            if(PRINTS & PRINT_CONN_ERR)fprintf(stderr, "init_server_tcp: Error in connect, %d\n", status);
            close(*server_fd);
            continue;
        }

        break;
    }

    if (p == NULL){
        if(PRINTS & PRINT_CONN_ERR)fprintf(stderr, "init_server_tcp: Error connecting to \"%s:%s\"\n", req_host, req_port);
        freeaddrinfo(serverinfo);
        return -1;
    }

    //fcntl(*server_fd, F_SETFL, O_NONBLOCK); // Set socket to non-blocking

    return 0;
}

void sigint_handler(int signum){
    printf("Exiting unsafely, please don't do this\n");
    printf("Instead, enter no command to exit ('enter')\n");
    exit(1);
}

void get_message(char * buf, int bytes){
    // Clear buffer
    memset(buf, 0, bytes);
    // Read input
    fgets(buf, bytes, stdin);
    // Make input null-terminating
    size_t bytes_no = strlen(buf);
    if(bytes_no != 0){
        buf[bytes_no-1] = '\0';
    }
}

void print_menu(){
    fprintf(stdout, "\nCommand options:\n");
    fprintf(stdout, "`list`      : List the files stored on DFS servers\n");
    fprintf(stdout, "`get <FILE>`: Download <FILE> from DFS servers\n");
    fprintf(stdout, "`put <FILE>`: Upload <FILE> onto DFS servers\n");
}

// Zero the values in servers
void zero_servers(Server * servers){
    for(int i = 0; i < 4; i++){
        memset(servers[i].ip, 0, MAX_IP);
        memset(servers[i].port, 0, MAX_PORT);
        memset(servers[i].dir, 0, MAX_DIR);
    }
}

// Check and initialize command line arguments
int init_cmdline(int argc, char * argv[], Server * servers){
    FILE * file;
    char line[MAX_LINE];
    void * rv;

    // Check for file name
    if(argc != 2){
        fprintf(stdout, "Usage %s <path/to/dfc.conf>\n", argv[0]);
        fprintf(stderr, "init_cmdline: Incorrect command line arguments\n");
        return -1;
    }

    // Check if config file exists
    if((file = fopen(argv[1], "r")) == NULL){
        fprintf(stderr, "init_cmdline: File `%s` cannot be opened\n", argv[1]);
        return -1;
    }

    zero_servers(servers);
    for(int i = 0; i < 4; i++){
        memset(line, 0, MAX_LINE);
        // Read line
        rv = fgets(line, MAX_LINE, file);
        if(rv == NULL){
            fprintf(stderr, "init_cmdline: Couldn't find all 4 lines in `%s`\n", argv[1]);
            fclose(file);
            return -1;
        }

        // Null terminating
        size_t bytes_no = strlen(line);
        if(bytes_no != 0){
            line[strlen(line)-1] = '\0';
        }

        //Update servers
        if(update_server_info(&(servers[i]), line, argv) < 0){
            fprintf(stderr, "init_cmdline: Failed adding server info to servers array\n");
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    return 0;
}

// Given a line from dfc.conf, entries in the server struct
int update_server_info(Server * server, char * line, char * argv[]){
    char * space1, * space2, * colon, * end;

    // Get character positions
    if((space1 = strchr(line, ' ')) == NULL){
        fprintf(stderr, "update_server_info: Failed to parse \"%s\" for space1\n", argv[1]);
        return -1;
    }
    if((space2 = strchr(space1 + 1, ' ')) == NULL){
        fprintf(stderr, "update_server_info: Failed to parse \"%s\" for space2\n", argv[1]);
        return -1;
    }
    if((colon = strchr(space2 + 1, ':')) == NULL){
        fprintf(stderr, "update_server_info: Failed to parse \"%s\" for colon\n", argv[1]);
        return -1;
    }
    if((end = strchr(colon + 1, '\0')) == NULL){
        fprintf(stderr, "update_server_info: Failed to parse \"%s\" for end\n", argv[1]);
        return -1;
    }

    // Extract values and put in correct servers location
    strncpy(server->dir, space1 + 1, space2 - space1 - 1);
    strncpy(server->ip, space2 + 1, colon - space2 - 1);
    strncpy(server->port, colon + 1, end - colon - 1);

    if(PRINTS & PRINT_UPDATE_SERVER)
        fprintf(stdout, "Adding server \"%s\" \"%s\":\"%s\"\n", server->dir, server->ip, server->port);

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
    sprintf(dirpath, "get_tmp/%s", dir);

    // Create the directory
    if(mkdir(dirpath, S_IRWXU) != 0 && errno != EEXIST){
        fprintf(stderr, "recv_file: Error in creating directory for \"%s\"\n", dirpath);
        return -1;
    }

    // Construct file path where we'll store the file
    char filepath[MAX_WRITE_SZ + 32];
    memset(filepath, 0, MAX_WRITE_SZ);
    sprintf(filepath, "%s/%d", dirpath, part_no);

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
