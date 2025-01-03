#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Constants
#define DOMAINLEN 256
#define BUFSIZE 512
#define PORTLEN 6 // "65536" + '\0'
#define FTP_PORT_STR "21"
                    
// Codes
#define HELLO_CODE 220
#define GOODBYE_CODE 220
#define WRONG_LOGIN_CODE 550
#define LOGGED_CODE 230
#define PASSWORD_REQUIRED_CODE 331
#define TRANSFER_COMPLETE_CODE 226
#define ENTERING_PASV_MODE_CODE 227
#define OPENING_BINARY_CONN_CODE 150

// Messages
#define MSG_220 "220 srvFtp version 1.0\r\n"
#define MSG_331 "331 Password required for %s\r\n"
#define MSG_230 "230 User %s logged in\r\n"
#define MSG_530 "530 Login incorrect\r\n"
#define MSG_221 "221 Goodbye\r\n"
#define MSG_550 "550 %s: no such file or directory\r\n"
#define MSG_299 "299 File %s size %ld bytes\r\n"
#define MSG_226 "226 Transfer complete\r\n"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define PROMPT "ftp> "

// Structured passed by function calls
struct conn_stats {
    int cmd_chnl;
    int passivemode;
};

/* Parses the pasv answer and calculates the port. 
 * The network address is not processed since it is always the server address.
 *
 * src: input string with the whole answer
 * dst: port obtained*/
void parse_pasvres (char *src, char *dst);

/**
 * function: receive and analize the answer from the server
 * sd: socket descriptor
 * code: three digit code to check if received
 * text: normally NULL. If a pointer is received as parameter
 *       then it is setted as the optional part of the message.
 * return: result of code checking
 **/
int recv_msg(int sd, int code, char *text);

/**
 * function: send command formated to the server
 * sd: socket descriptor
 * operation: four letters command
 * param: command parameters
 **/
void send_msg(int sd, char *operation, char *param);

/**
 * function: simple input from keyboard
 * return: input without \n character
 **/
char *read_input();

/**
 * function: login process from the client side
 * sd: socket descriptor
 **/
int authenticate(int sd);
/**
 * function: operation get
 * sd: socket descriptor
 * file_name: file name to get from the server
 **/
void get(char *file_name, struct conn_stats *stats);

/**
 * function: operation quit
 * sd: socket descriptor
 **/
void quit(int sd);

/**
 * function: make all operations (get|quit)
 * sd: socket descriptor
 **/
void operate(struct conn_stats*);

void ls(struct conn_stats*);

/* creates an active tcp socket.
 * stats: domain name of the service
 * port: service port
 * returns: socket file descriptor to the service */
int tcp_connection(const char* name, const char* port);

/* creates a passive tcp socket
 * name: domain name
 * port_str: 
 * returns: socket file descriptor of the created service */
int tcp_listen(char *port_str);

/* Setups passive or active data connection.
 * The opened file must be handled properly afterwards.
 * returns: data connection file descriptor, 0 if couldn't set connection*/
FILE *dataconn(struct conn_stats*, const char* mode);

void store(char *filename, struct conn_stats *stats);

/* obtains ip address from the sockaddr structure.
 *
 * sd: socket descriptor
 * dst: destiny string, should have a length of at least INET6_ADDRSTRLEN
 */
void ip_from_sd(int sd, char *dst);

/**
 * Run with
 *         ./myftp <SERVER_NAME> <SERVER_PORT>
 **/
int main(int argc, char *argv[]) 
{
    int sd;
    char *network_address, *port;
    struct conn_stats stats;

    // arguments checking
    if(argc <= 2) {
        fprintf(stderr, "Usage: %s [OPTION]... SERVER_NAME SERVER_PORT\n"
                "\t OPTION: -A, enable active mode\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    network_address = argv[argc - 2];
    port = argv[argc - 1];

    sd = tcp_connection(network_address, port);

    if(!sd) {
        fprintf(stderr, "main: Couldn't connect to server, name or port might be wrong\n");
        exit(EXIT_FAILURE);
    }
    // if receive hello proceed with authenticate and operate if not warning
    if(!recv_msg(sd, HELLO_CODE, NULL)) {
        fprintf(stderr, "main: Hello message not received, quiting client...\n");
        exit(EXIT_FAILURE);
        close(sd);
    }

    if(!authenticate(sd)) {
        exit(EXIT_FAILURE);
        close(sd);
    }

    stats.passivemode = 1;
    stats.cmd_chnl = sd;

    if(getopt(argc, argv, "A") == 'A') {
        stats.passivemode = 0;
    }

    operate(&stats);

    // close socket
    close(sd);

    return 0;
}

int recv_msg(int sd, int code, char *text) 
{
    char buffer[BUFSIZE], message[BUFSIZE];
    int recv_s, recv_code;

    // receive the answer
    recv_s = recv(sd, buffer, BUFSIZE, 0);
    // error checking
    if (recv_s == -1) {
        perror("recv");
        fprintf(stderr, "recv_msg: Error during recv");
    }

    if (recv_s == 0) {
        close(sd);
        fprintf(stderr, "recv_msg: Connection closed by host\n");
        exit(EXIT_FAILURE);
    }

    // parsing the code and message receive from the answer
    sscanf(buffer, "%d %[^\r\n]\r\n", &recv_code, message);
    printf("%d %s\n", recv_code, message);

    // optional copy of parameters
    if(text) strcpy(text, message);
    return code == recv_code;
}

void send_msg(int sd, char *operation, char *param) 
{
    char buffer[BUFSIZE] = {'\0'};

    // command formating
    if (param != NULL)
        sprintf(buffer, "%s %s\r\n", operation, param);
    else
        sprintf(buffer, "%s\r\n", operation);

    // send command and check for errors
    if(send(sd, buffer, strlen(buffer), 0) == -1) {
        fprintf(stderr, "couldn't send message\n");
    }

}

char *read_input() {
    char *input = malloc(BUFSIZE);
    if (fgets(input, BUFSIZE, stdin)) {
        return strtok(input, "\n");
    }
    return NULL;
}

int authenticate(int sd) {
    char *input = NULL, desc[100];

    // ask for user
    printf("username: ");
    input = read_input();

    // send the command to the server
    send_msg(sd, "USER", input);
    // release memory
    free(input);
    input = NULL;

    // wait to receive password requirement and check for errors
    if(!recv_msg(sd, PASSWORD_REQUIRED_CODE, desc)) {
        fprintf(stderr, "authenticate: Abnormal flow: %s\n", desc);
        return 0;
    }

    // ask for password
    printf("password: ");
    input = read_input();

    // send the command to the server
    send_msg(sd, "PASS", input);

    // release memory
    free(input);

    // wait for the answer, process it and check for errors
    if(!recv_msg(sd, LOGGED_CODE, desc)) {
        fprintf(stderr, "authenticate: Incorrect credentials: %s\n", desc);
        return 0;
    }

    return 1;
}


void get(char *file_name,struct conn_stats *stats) {
    char buffer[BUFSIZE];
    int cmd_sd = stats -> cmd_chnl;
    char c;
    FILE *write_file;
    FILE *read_file;


    // error checking missing
    // setups data channel
    read_file = dataconn(stats, "r");

    // send the RETR command to the server
    send_msg(cmd_sd, "RETR", file_name);
    
    // check for the response
    recv_msg(cmd_sd, 0, buffer);

    // parsing the write_file size from the answer received
    // sscanf(buffer, "File %*s size %d bytes", &f_size);

    // open the write_file to write
    write_file = fopen(file_name, "w");
    // opens a duplicate of the data channel

    //receive the write_file
    while ((c = getc(read_file)) != EOF) {
        putc(c, write_file);
    }

    // close the files 
    fclose(write_file);
    fclose(read_file);

    // receive the OK from the server
    if(!recv_msg(stats -> cmd_chnl, TRANSFER_COMPLETE_CODE, NULL)) {
        fprintf(stderr, "get: Transfer complete code not received\n");
    }
}

void quit(int sd) {
    // send command QUIT to the client
    send_msg(sd, "QUIT", NULL);
    // receive the answer from the server
    if(!recv_msg(sd, GOODBYE_CODE, NULL)) {
        fprintf(stderr, "quit: Couldn't close server connection properly\n");
        close(sd);
        exit(EXIT_FAILURE);
    }

}

void operate(struct conn_stats *stats) {
    char *input, *op, *param;

    while (1) {
        printf("ftp> ");
        input = read_input();
        if (!input)
            continue; // avoid empty input
        op = strtok(input, " ");
        param = strtok(NULL, " ");

        if (strcmp(op, "get") == 0) {
            get(param, stats);
        } else if (strcmp(op, "quit") == 0) {
            quit(stats -> cmd_chnl);
            break;
        } else if (strcmp(op, "ls") == 0) {
            ls(stats);
        } else if(strcmp(op, "send") == 0) {
            store(param, stats);
        } else {
            // new operations in the future
            printf("TODO: unexpected command\n");
        }
        free(input);
    }
    free(input);
}

FILE *dataconn(struct conn_stats *stats, const char* mode) 
{
    char buff[BUFSIZE] = {'\0'},
         ip_addr[INET6_ADDRSTRLEN] = {'\0'},
         port_str[PORTLEN] = {'\0'};
    int data_sd, server_sd;
    int octal_0, octal_1, octal_2, octal_3;
    int port;
    struct sockaddr peer_addr;
    socklen_t peer_addrlen = sizeof(peer_addr);

    // error checking
    ip_from_sd(stats -> cmd_chnl, ip_addr);

    if(stats -> passivemode) {
        send_msg(stats -> cmd_chnl, "PASV", NULL);
        if(!recv_msg(stats -> cmd_chnl, ENTERING_PASV_MODE_CODE, buff)) {
            fprintf(stderr, "dataconn: Abnormal flow\n");
            return 0;
        }

        // error checking
        parse_pasvres(buff, port_str);

        data_sd = tcp_connection(ip_addr, port_str);

        if(!data_sd) {
            fprintf(stderr, "dataconn: couldn't setup data connection\n");
            return NULL;
        }

        return fdopen(data_sd, mode);
    }
    
    data_sd = tcp_listen(port_str);

    if(!data_sd) {
        fprintf(stderr, "dataconn: couldn't listen to tcp socket");
        return NULL;
    }

    port = atoi(port_str);

    sscanf(ip_addr, "%d.%d.%d.%d", &octal_0, &octal_1, &octal_2, &octal_3);

    sprintf(buff, "%d,%d,%d,%d,%d,%d", octal_0, octal_1, octal_2, octal_3, port / 256, port % 256);

    send_msg(stats -> cmd_chnl, "PORT", buff);
                                                            
    if(!recv_msg(stats -> cmd_chnl, 200, NULL)) {
        fprintf(stderr, "dataconn: 200 not received after PORT command");
        return NULL;
    }

    server_sd = accept(data_sd, &peer_addr, &peer_addrlen); // blocks while waiting for server connection

    return fdopen(server_sd, mode);
}

int tcp_connection (const char* name, const char* port) 
{
    struct addrinfo hints;
    struct addrinfo *results = NULL, *rp = NULL;
    int addrstate = 0, sd = 0;

    if(!name || !port) return 0;

    printf("name used: %s, port used: %s\n", name, port);

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0; // any protocol


    if((addrstate = getaddrinfo(name, port, &hints, &results)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addrstate));
        return 0;
    }

    // create socket and connect to the first addrinfo available on the list, check for errors
    for(rp = results; rp != NULL; rp = rp->ai_next) {
        sd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if(sd == -1) {
            perror("socket");
            continue;
        }

        if(connect(sd, rp->ai_addr, rp->ai_addrlen) != -1) {
            break;
        }

        perror("connect");
        close(sd);
    }

    freeaddrinfo(results);
    return sd;
}

int tcp_listen(char *port_str_dst) 
{
    struct addrinfo hints;
    struct addrinfo *results = NULL, *rp = NULL;
    int addrstate = 0, sd = 0, portmax = 1 << 16;
    char port_str[PORTLEN];

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = 0; // any protocol
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_canonname = NULL;

    for(int i = 1025; i < portmax; i++) {
        sprintf(port_str, "%d", i);

        if((addrstate = getaddrinfo(NULL, port_str, &hints, &results)) == 0){
            break;
        } 
    }

    if (addrstate != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addrstate));
        return 0;
    }

    // create socket and connect to the first addrinfo available on the list, check for errors
    for (rp = results; rp != NULL; rp = rp->ai_next) {
        sd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if(sd == -1) {
            perror("socket");
            continue;
        }

        if(bind(sd, rp->ai_addr, rp->ai_addrlen) != -1) {
            break;
        }

        perror("bind");
        close(sd);
    }

    freeaddrinfo(results);

    if(!rp) {
        fprintf(stderr, "tcp_listen: Couldn't bind\n");
        return 0;
    }

    if(listen(sd, 1) == -1) {
        perror("tcp_listen");
        close(sd);
        return 0;
    }

    if(port_str_dst) {
        strncpy(port_str_dst, port_str, PORTLEN);
    }

    return sd;

}

void parse_pasvres(char *src, char *dst) 
{
    // reference: Entering Passive Mode (x,x,x,x,y,y)
    char buf[PORTLEN];
    char *inside_parentheses;
    int port_lowerbits = 0;
    int port_upperbits = 0;

    strtok(src,"()");
    inside_parentheses = strtok(NULL,"()");

    sscanf(inside_parentheses, "%*d,%*d,%*d,%*d,%d,%d", &port_upperbits, &port_lowerbits);
    sprintf(buf, "%d", port_upperbits * 256 + port_lowerbits);
    strncpy(dst, buf, PORTLEN);
}

void ls(struct conn_stats *stats) 
{
    FILE *data = NULL;
    char buffer[BUFSIZE] = {'\0'};

    data = dataconn(stats, "r");

    send_msg(stats -> cmd_chnl, "LIST", NULL);

    recv_msg(stats -> cmd_chnl, 0, buffer);

    while(!feof(data)) {
        fread(buffer, sizeof(char), BUFSIZE, data);
        printf("%s", buffer);
        memset(buffer, 0, BUFSIZE);
    }

    fclose(data);

    if(!recv_msg(stats -> cmd_chnl, TRANSFER_COMPLETE_CODE, NULL)) {
        fprintf(stderr, "get: Transfer complete code not received\n");
    }

}

void store(char *filename, struct conn_stats *stats) 
{
    FILE *data = NULL, *send_file;
    char buffer[BUFSIZE] = {'\0'};

    data = dataconn(stats, "w");

    send_file = fopen(filename, "r");

    send_msg(stats -> cmd_chnl, "STOR", filename);

    recv_msg(stats -> cmd_chnl, 0, buffer);

    while(!feof(send_file)) {
        fread(buffer, sizeof(char), BUFSIZE, send_file);
        fwrite(buffer, sizeof(char), BUFSIZE, data);
    }

    fclose(data);

    if(!recv_msg(stats -> cmd_chnl, TRANSFER_COMPLETE_CODE, NULL)) {
        fprintf(stderr, "get: Transfer complete code not received\n");
    }
}

void ip_from_sd(int sd, char *dst) 
{
    struct sockaddr addr;
    socklen_t addrlen = sizeof(addr);
    int af;

    if(!sd || !dst){
        fprintf(stderr, "ip_from_sd: invalid arguments\n");
        return;
    }

    if(getpeername(sd, &addr, &addrlen) == -1) {
        fprintf(stderr, "getpeername");
        return;
    }

    af = addr.sa_family;

    if (af == AF_INET) {
        inet_ntop(af, &((struct sockaddr_in*) &addr) -> sin_addr, dst, INET_ADDRSTRLEN);
    } else {
        inet_ntop(af, &((struct sockaddr_in6*) &addr) -> sin6_addr, dst, INET6_ADDRSTRLEN);
    }
}
