#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

char *get_next_header(char *req_buf, char *hdr);
int get_keep_alive(char *hdr);

int main(int argc, char * argv[]){
    char * str = "GET GET POST POST GET /1 HTTP/1.1\r\nHost: localhost:5000\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:85.0) Gecko/20100101 Firefox/85.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\nAccept-Language: en-US,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nDNT: 1\r\nConnection: close\r\nUpgrade-Insecure-Requests: 1\r\nCache-Control: max-age=0\r\n\r\nPOST /2 HTTP/1.1\r\nHost: localhost:5000\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:85.0) Gecko/20100101 Firefox/85.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\nAccept-Language: en-US,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nDNT: 1\r\nConnection: keep-alive\r\nUpgrade-Insecure-Requests: 1\r\nCache-Control: max-age=0\r\n\r\nPOST /3 HTTP/1.1\r\nHost: localhost:5000\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:85.0) Gecko/20100101 Firefox/85.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\nAccept-Language: en-US,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nDNT: 1\r\nUpgrade-Insecure-Requests: 1\r\nCache-Control: max-age=0\r\n\r\nGET /4 HTTP/1.1\r\nHost: localhost:5000\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:85.0) Gecko/20100101 Firefox/85.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\nAccept-Language: en-US,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nDNT: 1\r\nConnection: keep-alive\r\nUpgrade-Insecure-Requests: 1\r\nCache-Control: max-age=0\r\n\r\n";
    printf("Will be parsing the following:\n");
    printf("------------------------------\n");
    printf("%s", str);
    printf("------------------------------\n");
    char *hdr = NULL;
    while((hdr = get_next_header(str, hdr)) != NULL){
        // Print line of the header
        // Get location of \r
        char *loc = strchr(hdr, '\r');
        // Create substring
        char *buf = malloc(128);
        memset(buf, 0, 128);
        strncpy(buf, hdr, loc - hdr);
        printf("(%d) %s\n", get_keep_alive(hdr), buf);
    }
    hdr = NULL;
    while((hdr = get_next_header(str, hdr)) != NULL){
        // Print line of the header
        // Get location of \r
        char *loc = strchr(hdr, '\r');
        // Create substring
        char *buf = malloc(128);
        memset(buf, 0, 128);
        strncpy(buf, hdr, loc - hdr);
        printf("(%d) %s\n", get_keep_alive(hdr), buf);
    }
    hdr = NULL;
    char * str1 = "GET /10 HTTP/1.1\r\nHost: localhost:5000\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:85.0) Gecko/20100101 Firefox/85.0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\nAccept-Language: en-US,en;q=0.5\r\nAccept-Encoding: gzip, deflate\r\nDNT: 1\r\nConnection: close\r\nUpgrade-Insecure-Requests: 1\r\nCache-Control: max-age=0\r\n\r\n";
    printf("Will be parsing the following:\n");
    printf("------------------------------\n");
    printf("%s", str1);
    printf("------------------------------\n");
    while((hdr = get_next_header(str1, hdr)) != NULL){
        // Print line of the header
        // Get location of \r
        char *loc = strchr(hdr, '\r');
        // Create substring
        char *buf = malloc(128);
        memset(buf, 0, 128);
        strncpy(buf, hdr, loc - hdr);
        printf("(%d) %s\n", get_keep_alive(hdr), buf);
    }
}

// Returns 1 if "Connection: keep-alive" is found in current header, 0 otherwise
int get_keep_alive(char *hdr){
    char *hdr_end = strstr(hdr, "\r\n\r\n");
    char *next_conn = strstr(hdr, "Connection: keep-alive");

    if(next_conn == NULL || next_conn > hdr_end){
        return 0;
    }

    return 1;
}

// sets hdr to a char * to the beginning of the next HTTP command in req_buf, NULL otherwise
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