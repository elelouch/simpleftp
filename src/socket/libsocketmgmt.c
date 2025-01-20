#include "socketmgmt.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

int tcp_listen(char *port, int queue_size, struct sockaddr_storage *sas) 
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

// int get_ip_port(struct sockaddr* addr, char *dst, int *port)
// {
//     int af = 0;
// 
//     if(!addr) {
//         fprintf(stderr, "get_ip_port: invalid arguments\n");
//         return -1;
//     }
// 
//     af = addr->sa_family;
// 
//     if (af == AF_INET) {
//         struct sockaddr_in *s = (struct sockaddr_in*) addr;
//         if(port) *port = htons(s -> sin_port);
// 
//         if(dst && !inet_ntop(af, &s -> sin_addr, dst, INET_ADDRSTRLEN)){
//             perror("inet_ntop");
//             return -1;
//         }
//     } else {
//         struct sockaddr_in6 *s = (struct sockaddr_in6*) &addr;
// 
//         if(port) *port = htons(s -> sin6_port);
// 
//         if(dst && !inet_ntop(af, &s -> sin6_addr, dst, INET6_ADDRSTRLEN)){
//             perror("inet_ntop");
//             return -1;
//         }
//     }
// 
//     return 0;
// }
// 
// int socketinfo(int sd, char *ip_dst, int *port_ip_dst, int peer) 
// {
//     struct sockaddr addr;
//     socklen_t addrlen = sizeof(addr);
//     int res;
// 
//     if(!sd){
//         fprintf(stderr, "ip_from_sd: sd not valid\n");
//         return -1;
//     }
//     
//     res = peer ? 
//         getpeername(sd, &addr, &addrlen) :
//         getsockname(sd, &addr, &addrlen);
// 
// 
//     if(res == -1) {
//         perror("socketinfo");
//         return -1;
//     }
// 
//     return get_ip_port(&addr, ip_dst, port_ip_dst);
// }

int tcp_connection (const char* name, const char* port, struct sockaddr_storage *sas) 
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
            memcpy(sas, rp -> ai_addr, rp -> ai_addrlen);
            break;
        }

        perror("connect");
        close(sd);
    }

    freeaddrinfo(results);
    return sd;
}

