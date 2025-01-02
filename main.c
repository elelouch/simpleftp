#include <err.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define ARR_SIZE(arr) sizeof(arr)/sizeof(arr[0])

struct pedro {
    int hola;
    int chau;
};
#define HOLA " "

int main(int argc, char *argv[])
{
    char buffer[256];
    char hola[] = "eliasrojas\n";
    int addrstate, sd;
    struct addrinfo hints, *rp, *results;

    struct sockaddr *addr;
    struct in_addr *inet_addr;
    struct in6_addr *inet6_addr;
    int af;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    addrstate = getaddrinfo("localhost", "21", &hints, &results);

    if(addrstate != 0) {
        printf("error");
        return 1;
    }

    for(rp = results; rp != NULL; rp = rp -> ai_next) {
        sd = socket(rp -> ai_family, rp -> ai_socktype, rp -> ai_protocol);
        if(!sd) {
            fprintf(stderr,"error\n");
            return 1;
        }

        if(connect(sd, rp -> ai_addr, rp -> ai_addrlen) != -1) {
            break;
        }

        perror("continue");
        close(sd);
    }


    addr = rp -> ai_addr;
    af = rp -> ai_family;

    if(af == AF_INET) {
        inet_addr = &((struct sockaddr_in*) addr) -> sin_addr;
        printf("patient: %p\n", inet_ntop(af, inet_addr, buffer, INET_ADDRSTRLEN));
    } else {
        inet6_addr = &((struct sockaddr_in6*) addr) -> sin6_addr;
        printf("patient: %p\n", inet_ntop(af, inet6_addr, buffer, INET6_ADDRSTRLEN));
    }

    freeaddrinfo(results);
    printf("%s\n", buffer);


    return 0;
}
