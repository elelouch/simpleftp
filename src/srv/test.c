#include "../socket/socketmgmt.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    struct sockaddr_in soca;
    tcp_listen("0", 1, (struct sockaddr*) &soca);
    printf("soca port: %d\n", ntohs(soca.sin_port));
    return 0;
}
