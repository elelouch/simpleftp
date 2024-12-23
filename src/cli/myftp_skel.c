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
#define HELLO_CODE 220
#define GOODBYE_CODE 220
#define WRONG_LOGIN_CODE 550
#define LOGGED_CODE 230
#define PASSWORD_REQUIRED_CODE 230

#define MSG_220 "220 srvFtp version 1.0\r\n"
#define MSG_331 "331 Password required for %s\r\n"
#define MSG_230 "230 User %s logged in\r\n"
#define MSG_530 "530 Login incorrect\r\n"
#define MSG_221 "221 Goodbye\r\n"
#define MSG_550 "550 %s: no such file or directory\r\n"
#define MSG_299 "299 File %s size %ld bytes\r\n"
#define MSG_226 "226 Transfer complete\r\n"

/**
 * function: receive and analize the answer from the server
 * sd: socket descriptor
 * code: three leter numerical code to check if received
 * text: normally NULL but if a pointer if received as parameter
 *       then a copy of the optional message from the response
 *       is copied
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
void get(int sd, char *file_name);

/**
 * function: operation quit
 * sd: socket descriptor
 **/
void quit(int sd);

/**
 * function: make all operations (get|quit)
 * sd: socket descriptor
 **/
void operate(int sd);

/**
 * Run with
 *         ./myftp <SERVER_IP> <SERVER_PORT>
 **/
int main (int argc, char *argv[]) 
{
    int sd, addrstate;
    struct addrinfo hints, *results, *rp;

    // arguments checking
    if(argc != 3) {
        fprintf(stderr, "Usage: %s <SERVER_IP> <SERVER_PORT>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0; // any protocol

    addrstate = getaddrinfo(argv[1], argv[2], &hints, &results);

    if(!addrstate) {
        fprintf(stderr, "Get address info, error: %s\n", gai_strerror(addrstate));
        exit(EXIT_FAILURE);
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
    // connect and check for errors

    // if receive hello proceed with authenticate and operate if not warning
    if(!recv_msg(sd, HELLO_CODE, NULL)) {
        fprintf(stderr, "Hello message not received, quiting client...\n");
        close(sd);
    }

    if(!authenticate(sd)) {
        fprintf(stderr, "Couldn't authenticate, quitting...\n");
        exit(EXIT_FAILURE);
        close(sd);
    }

    operate(sd);

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
    if (recv_s == -1) warn("error receiving data");

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

    // wait to receive password requirement and check for errors
    if(!recv_msg(sd, PASSWORD_REQUIRED_CODE, desc)) {
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

void get(int sd, char *file_name) {
    char desc[BUFSIZE], buffer[BUFSIZE];
    int f_size, recv_s, r_size = BUFSIZE;
    FILE *file;

    // send the RETR command to the server

    // check for the response

    // parsing the file size from the answer received
    // "File %s size %ld bytes"
    sscanf(buffer, "File %*s size %d bytes", &f_size);

    // open the file to write
    file = fopen(file_name, "w");

    //receive the file

    // close the file
    fclose(file);

    // receive the OK from the server

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

void operate(int sd) {
    char *input, *op, *param;

    while (1) {
        printf("Operation: ");
        input = read_input();
        if (!input)
            continue; // avoid empty input
        op = strtok(input, " ");
        // free(input);
        if (strcmp(op, "get") == 0) {
            param = strtok(NULL, " ");
            get(sd, param);
        } else if (strcmp(op, "quit") == 0) {
            quit(sd);
            break;
        } else {
            // new operations in the future
            printf("TODO: unexpected command\n");
        }
        free(input);
    }
    free(input);
}
