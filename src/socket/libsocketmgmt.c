#include "socketmgmt.h"
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#define PORTLEN 6

int tcp_listen(char *port, int queue_size) 
{
    struct addrinfo hints;
    struct addrinfo *results = NULL, *rp = NULL;
    int addrstate = 0, sd = 0;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = 0; // any protocol
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_canonname = NULL;

    addrstate = getaddrinfo(NULL, port, &hints, &results);

    if (addrstate != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addrstate));
        return 0;
    }

    // create socket and connect to the first addrinfo available on the list, check for errors
    for (rp = results; rp != NULL; rp = rp->ai_next) {
        sd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if(sd == -1) {
            perror("socket");
            continue;
        }

        if(bind(sd, rp->ai_addr, rp->ai_addrlen) != -1) {
            break;
        }

        perror("bind");
        close(sd);
    }

    freeaddrinfo(results);

    if(!rp) {
        fprintf(stderr, "tcp_listen: Couldn't bind\n");
        return 0;
    }

    if(listen(sd, queue_size) == -1) {
        perror("tcp_listen");
        close(sd);
        return 0;
    }


    return sd;

}

void ip_from_sd(int sd, char *dst) 
{
    struct sockaddr addr;
    socklen_t addrlen = sizeof(addr);
    int af = 0;

    if(!sd || !dst){
        fprintf(stderr, "ip_from_sd: invalid arguments\n");
        return;
    }

    if(getpeername(sd, &addr, &addrlen) == -1) {
        perror("getpeername");
        return;
    }

    af = addr.sa_family;

    if (af == AF_INET) {
        inet_ntop(af, &((struct sockaddr_in*) &addr) -> sin_addr, dst, INET_ADDRSTRLEN);
    } else {
        inet_ntop(af, &((struct sockaddr_in6*) &addr) -> sin6_addr, dst, INET6_ADDRSTRLEN);
    }
}

int tcp_connection (const char* name, const char* port) 
{
    struct addrinfo hints;
    struct addrinfo *results = NULL, *rp = NULL;
    int addrstate = 0, sd = 0;

    if(!name || !port) return 0;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0; // any protocol

    if((addrstate = getaddrinfo(name, port, &hints, &results)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addrstate));
        return 0;
    }

    // create socket and connect to the first addrinfo available on the list, check for errors
    for(rp = results; rp != NULL; rp = rp->ai_next) {
        sd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if(sd == -1) {
            perror("socket");
            continue;
        }

        if(connect(sd, rp->ai_addr, rp->ai_addrlen) != -1) {
            break;
        }

        perror("connect");
        close(sd);
    }

    freeaddrinfo(results);
    return sd;
}

