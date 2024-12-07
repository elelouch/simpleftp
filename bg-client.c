#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

//                      -------- 
//                     |        |
//                     v        |
// socket -> bind -> listen -> accept // server
//
// socket -> bind -> connect // client
struct pedrinho {
    int val;
};

int main (int argc, char *argv[]) 
{
    int sd = 0;
    struct sockaddr_in my_addr;
    struct sockaddr_in server_addr;

    int port = 0;

    if (argc != 3) {
        fprintf(stderr, "Usage: client CLI_IPADDR CLI_PORT");
        exit(1);
    }

    sd = socket(AF_INET, SOCK_STREAM, 0);
    port = atoi(argv[2]);

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = inet_addr(argv[1]);
    memset(&my_addr.sin_addr, '\0', sizeof(my_addr.sin_zero));

    if(bind(sd, (struct sockaddr*) &my_addr, sizeof(my_addr)) == -1) {
        close(sd);
        fprintf(stderr, "Could not bind address used\n");
        exit(1);
    }

    return 0;
}
