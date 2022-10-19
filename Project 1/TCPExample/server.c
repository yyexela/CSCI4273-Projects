#include <stdio.h>
#include <sys/types.h> // Only needed if portable
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h> 

int main(int argc, char * argv){
    int s;
    int c;
    struct addrinfo hints;
    struct addrinfo *serverinfo, *p;
    struct sockaddr_storage clientinfo;
    int clientinfo_size = sizeof(clientinfo);
    int status;
    void * buf;
    int yes = 1;

    printf("Running Server\n");

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if((status = getaddrinfo(NULL, "3490", &hints, &serverinfo)) != 0){
        printf("Error in getaddrinfo\n");
        exit(1);
    }

    for(p = serverinfo; p != NULL; p = p->ai_next){
        if((s = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            printf("Error in socket\n");
            continue;
        }

        if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
            printf("Error in setsockopt\n");
            exit(1);
        }

        if((status = bind(s, p->ai_addr, p->ai_addrlen)) == -1){
            close(s);
            printf("Error in bind\n");
            continue;
        }
        break;
    }

    if(p == NULL){
        printf("ERROR: p == NULL\n");
        freeaddrinfo(serverinfo);
        exit(1);
    }


    if((status = listen(s, 5)) == -1){
        printf("Error in listen\n");
        exit(1);
    }

    buf = malloc(100);

    while(1){
        if((c = accept(s, (struct sockaddr *) &clientinfo, &clientinfo_size)) == -1){
            printf("Error in accept\n");
            continue;
        }

        if((status = recv(c, buf, 100, 0)) == -1){
            printf("Error in recv %d %s\n", status, gai_strerror(status));
            exit(1);
        } else {
            printf("%s\n", buf);
            break;
        }
    }


    close(s);

    free(buf);
}