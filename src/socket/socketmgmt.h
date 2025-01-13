#ifndef _SOCKET_MGMT_H_
#define _SOCKET_MGMT_H_
/* creates an active tcp socket.
 * stats: domain name of the service
 * port: service port
 * returns: socket file descriptor to the service */
int tcp_connection(const char* name, const char* port);

/* Creates a passive tcp socket.
 * name: domain name
 * port_str: Optional string that will contain the port that the service is using
 * returns: socket file descriptor of the created service 
 */
int tcp_listen(char *port, int queue_size);

/* obtains ip address from the socket file descriptor
 * sd: socket descriptor
 * dst: destiny buffer, should have a length of at least INET6_ADDRSTRLEN
 */
void ip_from_sd(int sd, char *dst);
#endif
