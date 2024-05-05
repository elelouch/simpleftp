#ifndef TCP_UTILS
#define TCP_UTILS
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>

/* Takes an IPV4 address, a port, and a sockaddr structure.
 * Initalizes the sockaddr structure.
 * Returns the socket fd or -1 on error
 * */ 
int init_sockaddr_in(char* ip_addr, int port, struct sockaddr_in* addr);
#endif
