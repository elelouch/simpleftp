#include "socketmgmt.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

int tcp_listen(char *port, int queue_size, struct sockaddr *sas) 
{
    struct addrinfo hints;
    struct addrinfo *results = NULL, *rp = NULL;
    int addrstate = 0, sd = 0;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET; // just IPV4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = 0; // any protocol
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_canonname = NULL;

    addrstate = getaddrinfo(NULL, port, &hints, &results);

    if (addrstate != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addrstate));
        return -1;
    }

    // create socket and connect to the first addrinfo available on the list, check for errors
    for (rp = results; rp != NULL; rp = rp->ai_next) {
        sd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if(sd == -1) {
            perror("socket");
            continue;
        }

        if(bind(sd, rp->ai_addr, rp->ai_addrlen) != -1) {
            memcpy(sas, rp -> ai_addr, rp -> ai_addrlen);
            break;
        }

        perror("bind");
        close(sd);
    }

    freeaddrinfo(results);

    if(!rp) {
        fprintf(stderr, "tcp_listen: Couldn't bind\n");
        return -1;
    }

    if(listen(sd, queue_size) == -1) {
        perror("tcp_listen");
        close(sd);
        return -1;
    }

    return sd;
}

int tcp_connection (const char* name, const char* port, struct sockaddr *addr) 
{
    struct addrinfo hints;
    struct addrinfo *results = NULL, *rp = NULL;
    int addrstate = 0, sd = 0;

    if(!name || !port) return -1;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET; // Just IPV4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0; // any protocol

    if((addrstate = getaddrinfo(name, port, &hints, &results)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addrstate));
        return -1;
    }

    // create socket and connect to the first addrinfo available on the list, check for errors
    for(rp = results; rp != NULL; rp = rp->ai_next) {
        sd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if(sd == -1) {
            perror("socket");
            continue;
        }

        if(connect(sd, rp->ai_addr, rp->ai_addrlen) != -1) {
            memcpy(addr, rp -> ai_addr, rp -> ai_addrlen);
            break;
        }

        perror("connect");
        close(sd);
    }

    freeaddrinfo(results);
    return sd;
}

int is_ipv6(struct sockaddr_util *sau) 
{
    return sau -> u.sa.sa_family == AF_INET6;
}
