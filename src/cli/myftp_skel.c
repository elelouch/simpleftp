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

#define BUFSIZE 512
#define PORT_SIZE 6 // "65536" + '\0'
#define HELLO_CODE 220
#define GOODBYE_CODE 220
#define WRONG_LOGIN_CODE 550
#define LOGGED_CODE 230
#define NEED_PASSWORD_CODE 331

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

struct conn_stats {
    char ip_addr[BUFSIZE];
    char data_port[PORT_SIZE];
    char cmd_port[PORT_SIZE];
    int passivemode;
    int data_chnl;
    int cmd_chnl;
};

/* Parses the pasv answer and calculates the port. 
 * The network address is not processed since it is always the server address.
 *
 * src: input string with the whole answer
 * dst: port obtained*/
void parse_pasvres (char *src, char *dst);

void close_dataconn (struct conn_stats* stats);
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

/**
 * Run with
 *         ./myftp <SERVER_IP> <SERVER_PORT>
 **/

FILE* dataconn (const char*, int, int);

int tcp_connection (const char* ip, const char* port);

int setup_dataconn(struct conn_stats*);

int main (int argc, char *argv[]) 
{
    int sd;
    struct conn_stats program_stats;

    // arguments checking
    if(argc < 2) {
        fprintf(stderr, "Usage: %s <SERVER_NAME> [<SERVER_PORT>]\n", argv[0]);
        exit(EXIT_FAILURE);
    }


    if (argc == 2) {
        sd = tcp_connection(argv[1], "21");
    }

    if (argc == 3) {
        sd = tcp_connection(argv[1], argv[2]);
    }


    if(!sd) {
        fprintf(stderr, "Couldn't connect to server\n");
        exit(EXIT_FAILURE);
    }
    // if receive hello proceed with authenticate and operate if not warning
    if(!recv_msg(sd, HELLO_CODE, NULL)) {
        fprintf(stderr, "Hello message not received, quiting client...\n");
        exit(EXIT_FAILURE);
        close(sd);
    }

    if(!authenticate(sd)) {
        fprintf(stderr, "Couldn't authenticate, quitting...\n");
        exit(EXIT_FAILURE);
        close(sd);
    }

    program_stats.cmd_chnl = sd;
    program_stats.data_chnl = 0;
    program_stats.passivemode = 1;
    strncpy(program_stats.ip_addr, argv[1], BUFSIZE);
// strncpy(program_stats.cmd_port, argv[2], PORT_SIZE);
    
    operate(&program_stats);

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
        fprintf(stderr, "Error during recv");
    }

    if (recv_s == 0) {
        close(sd);
        fprintf(stderr, "connection closed by host\n");
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
    char buffer[BUFSIZE] = "";

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
    int code;

    // ask for user
    printf("username: ");
    input = read_input();

    // send the command to the server
    send_msg(sd, "USER", input);
    // release memory
    free(input);
    input = NULL;

    // wait to receive password requirement and check for errors
    if(!recv_msg(sd, NEED_PASSWORD_CODE, desc)) {
        fprintf(stderr, "Abnormal flow: %s\n", desc);
        return 0;
    }

    // ask for password
    printf("passwd: ");
    input = read_input();

    // send the command to the server
    send_msg(sd, "PASS", input);

    // release memory
    free(input);

    // wait for answer and process it and check for errors
    if(!recv_msg(sd, LOGGED_CODE, desc)) {
        fprintf(stderr, "Incorrect credentials: %s\n", desc);
        return 0;
    }

    return 1;
}

void get(char *file_name,struct conn_stats *stats) {
    char desc[BUFSIZE], buffer[BUFSIZE];
    int f_size, recv_s, r_size = BUFSIZE;
    char c;
    FILE *file;
    FILE *file_received;

    // send the RETR command to the server
    send_msg(stats -> cmd_chnl, "RETR", file_name);
    // check for the response
    recv_msg(stats -> cmd_chnl, 226, buffer);
    // parsing the file size from the answer received
    sscanf(buffer, "File %*s size %d bytes", &f_size);

    // open the file to write
    file = fopen(file_name, "w");
    file_received = fdopen(stats -> data_chnl, "r");

    //receive the file
    while ((c = getc(file_received)) != EOF) {
        putc(c, file);
    }

    // close the file
    fclose(file);
    fclose(file_received);

    // receive the OK from the server
    if(!recv_msg(stats -> cmd_chnl, 220, NULL)) {
        fprintf(stderr, "Error receiving value\n");
    }
}

void quit(int sd) {
    // send command QUIT to the client
    send_msg(sd, "QUIT", NULL);
    // receive the answer from the server
    if(!recv_msg(sd, GOODBYE_CODE, NULL)) {
        fprintf(stderr, "couldn't close server connection properly\n");
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
        // free(input);
        if (strcmp(op, "get") == 0) {
            // use data transfer process before (my comm)
            param = strtok(NULL, " ");

            if(!setup_dataconn(stats)) continue;
            get(param, stats);
            close_dataconn(stats);
        } else if (strcmp(op, "quit") == 0) {
            quit(stats -> cmd_chnl);
            break;
        } else {
            // new operations in the future
            printf("TODO: unexpected command\n");
        }
        free(input);
    }
    free(input);
}

/* setup passive or active data connection */
int setup_dataconn (struct conn_stats *stats) 
{
    char buff[BUFSIZE];
    char port[PORT_SIZE];
    char *passive_mode_res;
    
    int upper_port;
    int lower_port;

    if(stats -> passivemode) {
        send_msg(stats -> cmd_chnl, "PASV", NULL);

        if(!recv_msg(stats -> cmd_chnl, 227, buff)) {
            return 0;
        }

        parse_pasvres(buff, stats -> data_port);

        stats -> data_chnl = tcp_connection(stats -> ip_addr, stats -> data_port);

        return stats -> data_chnl;
    }

    fprintf(stderr, "Couldn't initialize data channel\n");

    return 0;
}

/* return socket of a tcp connection to the server */
int tcp_connection (const char *ip, const char *port) 
{
    struct addrinfo hints;
    struct addrinfo *results, *rp;
    uint16_t u_port = 0;
    int addrstate = 0;
    int sd = 0;

    if(!ip || !port)  {
        return 0;
    }
    printf("ip used: %s, port used: %s\n", ip, port);

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0; // any protocol


    addrstate = getaddrinfo(ip, port, &hints, &results);

    if(addrstate) {
        fprintf(stderr, "getaddrinfo, error: %s\n", gai_strerror(addrstate));
        return 0;
    }

    // create socket and check for errors
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

void close_dataconn(struct conn_stats *stats) 
{
    close(stats -> data_chnl);
}

void parse_pasvres(char *src, char *dst) 
{
    // reference: Entering Passive Mode (x,x,x,x,y,y)
    char buf[PORT_SIZE];
    char *inside_parentheses;
    int port_lowerbits = 0;
    int port_upperbits = 0;

    strtok(src,"()");
    inside_parentheses = strtok(NULL,"()");

    sscanf(inside_parentheses, "%*d,%*d,%*d,%*d,%d,%d", &port_upperbits, &port_lowerbits);
    sprintf(buf, "%d", port_upperbits * 256 + port_lowerbits);
    strncpy(dst, buf, PORT_SIZE);
}
