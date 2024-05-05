#include "tcp_utils.h"

int init_sockaddr_in(char* ip_addr, int port, struct sockaddr_in* addr){
    int sd;
    int bind_status;

    if(!addr){
        return -1;
    }

    sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sd == - 1) {
        perror("socket");
        return -1;
    }

    if(ip_addr) {
        addr->sin_addr.s_addr = inet_addr(ip_addr);
    }
    addr->sin_port = htons(port);
    addr->sin_family = AF_INET;
    memset(addr->sin_zero,'\0',sizeof addr->sin_zero);

    bind_status = bind(sd, (struct sockaddr*) addr, sizeof *addr);
    if(bind_status == -1) {
        perror("bind");
        close(sd);
        return -1;
    }
    return sd;
}
