#include <sys/socket.h>
#include <netinet/in.h>

#ifndef _SOCKET_MGMT_H_
#define _SOCKET_MGMT_H_

#define PEER_INFO 1
#define CURRENT_INFO 0
#define PORTLEN 6


struct sockaddr_util {
    union {
        struct sockaddr sa;
        struct sockaddr_in sa4;
        struct sockaddr_in6 sa6;
    } u;
};

/* Structure passed through function calls.
 * Why not using a global state? It is messy, every function can potentially change it.
 * Plus, it is difficult to remember whether a variable belongs to the global scope or local 
 * scope during the function definition.
 * This helps tracking changes.
 */
struct conn_stats {
    int cmd_chnl;
    int data_chnl;
    int passivemode;
    struct sockaddr_util sau_cmd;
    struct sockaddr_util sau_data;
    int verbose;
};
/* creates an active tcp socket.
 * stats: domain name of the service. Nullable.
 * port: service port, use "0" for picking a random port. Nullable.
 * returns: socket file descriptor to the service */
int tcp_connection(const char* name, const char* port, struct sockaddr *addr);

/* Creates a passive tcp socket.
 * name: domain name
 * port_str: Optional string that will contain the port that the service is using
 * returns: socket file descriptor of the created service 
 */
int tcp_listen(char *port, int queue_size, struct sockaddr *addr);

int is_ipv6(struct sockaddr_util *sa);

#endif
