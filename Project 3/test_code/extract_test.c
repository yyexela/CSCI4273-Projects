#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include "../server.h"

void clear_bufs(char *req_meth, char *req_uri, char *req_ver, char *req_host, char *req_port, char *req_buf){
    memset(req_meth, 0, MAX_METH); memset(req_uri, 0, MAX_URI);   memset(req_ver, 0, MAX_VER);
    memset(req_host, 0, MAX_HOST); memset(req_port, 0, MAX_PORT); memset(req_buf, 0, MAX_REQ);
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
    uintptr_t line_end_1 = loc - req_buf;

    // Ensure that the ordering of the first two spaces and the line end are what we expect
    if(space_1 >= space_2 || space_1 >= line_end_1 || space_2 >= line_end_1){
        fprintf(stderr, "extract_req_fields: ERROR finding the end of the first line in request\n");
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
                    fprintf(stderr, "extract_req_fields: ERROR finding first space in \"Host:\" line\n");
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
            fprintf(stderr, "extract_req_fields: ERROR finding second end-line\n");
            return -1;
        }
        // Find end of line
        if ((loc = strchr(req_buf + host_start + 1, '\r')) == NULL){
            fprintf(stderr, "extract_req_fields: ERROR finding second end-line\n");
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
            fprintf(stderr, "extract_req_fields: ERROR Port string too long\n");
        strncpy(req_port, req_buf + colon_1 + 1, host_end - colon_1 - 1);
        if(colon_1 - host_start - 1 >= MAX_HOST)
            fprintf(stderr, "extract_req_fields: ERROR Host string too long\n");
        strncpy(req_host, req_buf + host_start + 1, colon_1 - host_start - 1);
    } else {
        strcpy(req_port, "80");
        strncpy(req_host, req_buf + host_start + 1, host_end - host_start - 1);
    }

    // Fill in the request fields
    if(space_1 >= MAX_METH)
        fprintf(stderr, "extract_req_fields: ERROR method string too long\n");
    if(space_2 - space_1 - 1 >= MAX_URI)
        fprintf(stderr, "extract_req_fields: ERROR method string too long\n");
    if(line_end_1 - space_2 - space_1 - 1 >= MAX_VER)
        fprintf(stderr, "extract_req_fields: ERROR method string too long\n");
    strncpy(req_meth, req_buf, space_1);
    strncpy(req_uri, req_buf + space_1 + 1, space_2 - space_1 - 1);
    strncpy(req_ver, req_buf + space_2 + 1, line_end_1 - space_2 - 1);
    return 0;
}

int run_test(char *req, char *meth, char *uri, char *ver, char *host, char *port, char *req_buf, char *req_meth, char *req_uri, char *req_ver, char *req_host, char *req_port){
    clear_bufs(req_meth, req_uri, req_ver, req_host, req_port, req_buf);
    strcpy(req_buf, req);
    if(extract_req_fields(req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        printf("ERROR for input of \"%s\"\n", req);
        exit(0);
    } else {
        if(strcmp(req_meth,meth) != 0){
            printf("ERROR with req_meth: got \"%s\" expected \"%s\"\n", req_meth, meth);
            return -1;
        }
        if(strcmp(req_uri,uri) != 0){
            printf("ERROR with req_uri: got \"%s\" expected \"%s\"\n", req_uri, uri);
            return -1;
        }
        if(strcmp(req_ver,ver) != 0){
            printf("ERROR with req_ver: got \"%s\" expected \"%s\"\n", req_ver, ver);
            return -1;
        }
        if(strcmp(req_host,host) != 0){
            printf("ERROR with req_host: got \"%s\" expected \"%s\"\n", req_host, host);
            return -1;
        }
        if(strcmp(req_port,port) != 0){
            printf("ERROR with req_port: got \"%s\" expected \"%s\"\n", req_port, port);
            return -1;
        }
    }
    return 0;
}

int main(int argc, char * argv[]){
    char *req_buf;
    char req_meth[MAX_METH], req_uri[MAX_URI], req_ver[MAX_VER], req_host[MAX_HOST], req_port[MAX_PORT];
    req_buf = malloc(MAX_REQ);

    printf("Running no host test cases\n");

    // NO PORT, NO /
    if( run_test("GET google.com HTTP/1.1\r\n\r\n", "GET", "google.com", "HTTP/1.1", "google.com", "80",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET http://google.com HTTP/1.1\r\n\r\n", "GET", "http://google.com", "HTTP/1.1", "google.com", "80",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET www.google.com HTTP/1.1\r\n\r\n", "GET", "www.google.com", "HTTP/1.1", "www.google.com", "80",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET http://www.google.com HTTP/1.1\r\n\r\n", "GET", "http://www.google.com", "HTTP/1.1", "www.google.com", "80",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    // NO PORT, YES /
    if( run_test("GET google.com/ HTTP/1.1\r\n\r\n", "GET", "google.com/", "HTTP/1.1", "google.com", "80",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET http://google.com/ HTTP/1.1\r\n\r\n", "GET", "http://google.com/", "HTTP/1.1", "google.com", "80",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET www.google.com/ HTTP/1.1\r\n\r\n", "GET", "www.google.com/", "HTTP/1.1", "www.google.com", "80",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET http://www.google.com/ HTTP/1.1\r\n\r\n", "GET", "http://www.google.com/", "HTTP/1.1", "www.google.com", "80",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    // YES PORT, YES /
    if( run_test("GET google.com:1234/ HTTP/1.1\r\n\r\n", "GET", "google.com:1234/", "HTTP/1.1", "google.com", "1234",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET http://google.com:1234/ HTTP/1.1\r\n\r\n", "GET", "http://google.com:1234/", "HTTP/1.1", "google.com", "1234",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET www.google.com:1234/ HTTP/1.1\r\n\r\n", "GET", "www.google.com:1234/", "HTTP/1.1", "www.google.com", "1234",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET http://www.google.com:1234/ HTTP/1.1\r\n\r\n", "GET", "http://www.google.com:1234/", "HTTP/1.1", "www.google.com", "1234",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    // YES PORT, NO /
    if( run_test("GET google.com:1234 HTTP/1.1\r\n\r\n", "GET", "google.com:1234", "HTTP/1.1", "google.com", "1234",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET http://google.com:1234 HTTP/1.1\r\n\r\n", "GET", "http://google.com:1234", "HTTP/1.1", "google.com", "1234",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET www.google.com:1234 HTTP/1.1\r\n\r\n", "GET", "www.google.com:1234", "HTTP/1.1", "www.google.com", "1234",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET http://www.google.com:1234 HTTP/1.1\r\n\r\n", "GET", "http://www.google.com:1234", "HTTP/1.1", "www.google.com", "1234",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    printf("Running host test cases\n");

    // NO PORT, NO /
    if( run_test("GET google.com HTTP/1.1\r\nHost: google.com\r\n\r\n", "GET", "google.com", "HTTP/1.1", "google.com", "80",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET http://google.com HTTP/1.1\r\nHost: http://google.com\r\n\r\n", "GET", "http://google.com", "HTTP/1.1", "google.com", "80",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET www.google.com HTTP/1.1\r\nHost: www.google.com\r\n\r\n", "GET", "www.google.com", "HTTP/1.1", "www.google.com", "80",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET http://www.google.com HTTP/1.1\r\nHost: http://www.google.com\r\n\r\n", "GET", "http://www.google.com", "HTTP/1.1", "www.google.com", "80",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    // NO PORT, YES /
    if( run_test("GET google.com/ HTTP/1.1\r\nHost: google.com/\r\n\r\n", "GET", "google.com/", "HTTP/1.1", "google.com", "80",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET http://google.com/ HTTP/1.1\r\nHost: http://google.com/\r\n\r\n", "GET", "http://google.com/", "HTTP/1.1", "google.com", "80",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET www.google.com/ HTTP/1.1\r\nHost: www.google.com/\r\n\r\n", "GET", "www.google.com/", "HTTP/1.1", "www.google.com", "80",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET http://www.google.com/ HTTP/1.1\r\nHost: http://www.google.com\r\n\r\n", "GET", "http://www.google.com/", "HTTP/1.1", "www.google.com", "80",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    // YES PORT, YES /
    if( run_test("GET google.com:1234/ HTTP/1.1\r\nHost: google.com:1234/\r\n\r\n", "GET", "google.com:1234/", "HTTP/1.1", "google.com", "1234",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET http://google.com:1234/ HTTP/1.1\r\nHost: http://google.com:1234/\r\n\r\n", "GET", "http://google.com:1234/", "HTTP/1.1", "google.com", "1234",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET www.google.com:1234/ HTTP/1.1\r\nHost: www.google.com:1234/\r\n\r\n", "GET", "www.google.com:1234/", "HTTP/1.1", "www.google.com", "1234",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET http://www.google.com:1234/ HTTP/1.1\r\nHost: http://www.google.com:1234/\r\n\r\n", "GET", "http://www.google.com:1234/", "HTTP/1.1", "www.google.com", "1234",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    // YES PORT, NO /
    if( run_test("GET google.com:1234 HTTP/1.1\r\nHost: google.com:1234\r\n\r\n", "GET", "google.com:1234", "HTTP/1.1", "google.com", "1234",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET http://google.com:1234 HTTP/1.1\r\nHost: http://google.com:1234\r\n\r\n", "GET", "http://google.com:1234", "HTTP/1.1", "google.com", "1234",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET www.google.com:1234 HTTP/1.1\r\nHost: www.google.com:1234\r\n\r\n", "GET", "www.google.com:1234", "HTTP/1.1", "www.google.com", "1234",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    if( run_test("GET http://www.google.com:1234 HTTP/1.1\r\nHost: http://www.google.com:1234\r\n\r\n", "GET", "http://www.google.com:1234", "HTTP/1.1", "www.google.com", "1234",
        req_buf, req_meth, req_uri, req_ver, req_host, req_port) < 0){
        return 0;
    }

    printf("All test cases passed!\n");
}