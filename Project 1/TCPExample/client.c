#include <stdio.h>
#include <sys/types.h> // Only needed if portable
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char * argv){
    int s;
    struct addrinfo hints;
    struct addrinfo *serverinfo;
    struct addrinfo *p;
    int status;

    printf("Running Client\n");

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if((status = getaddrinfo("localhost", "3490", &hints, &serverinfo)) != 0){
        printf("Error in getaddrinfo\n");
        exit(1);
    }

    for(p = serverinfo; p != NULL; p = p->ai_next){
        printf("trying to connect to %s\n", p->ai_canonname);

        if((s = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            printf("Error in socket\n");
        }

        if((status = connect(s, p->ai_addr, p->ai_addrlen)) == -1){
            printf("Error in connect, %d\n", status);
            close(s);
            continue;
        }

        break;
    }

    if (p == NULL){
        printf("Error connecting\n");
        freeaddrinfo(serverinfo);
        exit(1);
    }

    if((status = send(s, "YO\0", 3, 0)) == -1){
        printf("Error sending\n");
    }

    printf("sent\n");

    freeaddrinfo(serverinfo);
}