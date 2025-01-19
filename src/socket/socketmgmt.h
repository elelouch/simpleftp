#ifndef _SOCKET_MGMT_H_
#define _SOCKET_MGMT_H_

#define PEER_INFO 1
#define CURRENT_INFO 0
#define PORTLEN 6

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

/* obtains ip address and port from the socket file descriptor
 * sd: socket descriptor. 
 * dst: optional buffer, should have a length of at least INET6_ADDRSTRLEN to place an address
 * peer: USE PEER_INFO OR CURRENT_INFO. if != 0, selects peer address, else selects socket address.
 * dst_port: optional integer to write the socket destiny.
 */
int socketinfo(int sd, char *dst, int *dst_port, int peer);

#endif
