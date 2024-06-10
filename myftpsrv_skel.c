#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <err.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>

// #include "tcp_utils.h"

#define BUFSIZE 512
#define CMDSIZE 4
#define PARSIZE 100
#define BACKLOG_SIZE 128
#define IPV6_LEN 128

#define MSG_220 "220 srvFtp version 1.0\r\n"
#define MSG_331 "331 Password required for %s\r\n"
#define MSG_230 "230 User %s logged in\r\n"
#define MSG_530 "530 Login incorrect\r\n"
#define MSG_221 "221 Goodbye\r\n"
#define MSG_550 "550 %s: no such file or directory\r\n"
#define MSG_299 "299 File %s size %ld bytes\r\n"
#define MSG_226 "226 Transfer complete\r\n"

void handle_connection(int, int);
void remove_last_two(char *);
/**
 * function: receive the commands from the client
 * sd: socket descriptor
 * operation: \0 if you want to know the operation received
 *            OP if you want to check an especific operation
 *            ex: recv_cmd(sd, "USER", param)
 * param: parameters for the operation involve
 * return: only usefull if you want to check an operation
 *         ex: for login you need the seq USER PASS
 *             you can check if you receive first USER
 *             and then check if you receive PASS
 **/
int recv_cmd(int sd, char *operation, char *param) {
    char buffer[BUFSIZE], *token;
    int recv_s;

    // receive the command in the buffer and check for errors



    // expunge the terminator characters from the buffer
    buffer[strcspn(buffer, "\r\n")] = 0;

    // complex parsing of the buffer
    // extract command receive in operation if not set \0
    // extract parameters of the operation in param if it needed
    token = strtok(buffer, " ");
    if (!token || strlen(token) < 4) {
        fprintf(stderr, "Not a valid ftp command\n");
        return 0;
    } 
    if (operation[0] == '\0') strcpy(operation, token);
    if (strcmp(operation, token)) {
        fprintf(stderr, "Abnormal client flow: did not send %s command\n", operation);
        return 0;
    }
    token = strtok(NULL, " ");
    if (token) strcpy(param, token);
    return 1;
}

/**
 * function: send answer to the client
 * sd: file descriptor
 * message: formatting string in printf format
 * ...: variable arguments for economics of formats
 * return: true if not problem arise or else
 * notes: the MSG_x have preformated for these use
 **/
int send_ans(int sd, char *message, ...){
    char buffer[BUFSIZE];

    va_list args;
    va_start(args, message);

    vsprintf(buffer, message, args);
    va_end(args);
    // send answer preformated and check errors

    return 0;
}

/**
 * function: RETR operation
 * sd: socket descriptor
 * file_path: name of the RETR file
 **/

void retr(int sd, char *file_path) {
    FILE *file;    
    int bread;
    long fsize;
    char buffer[BUFSIZE];

    // check if file exists if not inform error to client

    // send a success message with the file length

    // important delay for avoid problems with buffer size
    sleep(1);

    // send the file

    // close the file

    // send a completed transfer message
}
/**
 * funcion: check valid credentials in ftpusers file
 * user: login user name
 * pass: user password
 * return: true if found or false if not
 **/
int check_credentials(char *user, char *pass) {
    FILE *file;
    char *path = "./ftpusers", *line = NULL, credentials[100];
    size_t line_size = 0;
    int found = 0;

    // make the credential string
    sprintf(credentials, "%s:%s", user, pass);

    printf("credentials: %s\n", credentials);
    // check if ftpusers file it's present
    file = fopen(path, "r");
    if (!file) {
        warn("Error opening %s", path);
        return 0;
    }

    // search for credential string
    while (getline(&line, &line_size, file) != -1) {
        strtok(line, "\n");
        if (!strcmp(line, credentials)) {
            found = 1;
            break;
        }
    }

    // close file and release any pointers if necessary
    fclose(file);
    if (line) free(line);

    // return search status
    return found;
}

/**
 * function: login process management
 * sd: socket descriptor
 * return: true if login is succesfully, false if not
 **/
int authenticate(int sd) {
    int response_len = strlen(MSG_230) + PARSIZE;
    char user[PARSIZE];
    char pass[PARSIZE];
    char cli_msg[PARSIZE];
    char response[response_len];

    // remember to check for errors and exit if anything happens
    // wait to receive USER action
    do {
        recv(sd, cli_msg, PARSIZE, 0);
    } while(strcmp(cli_msg, "USER\r\n"));

    recv(sd, user, PARSIZE, 0);
    remove_last_two(user);
    do {
        recv(sd, cli_msg, PARSIZE, 0);
    } while(strcmp(cli_msg, "PASS\r\n"));

    recv(sd, pass, PARSIZE, 0);
    remove_last_two(pass);
    // wait to receive PASS action

    // if credentials don't check denied login
    if(!check_credentials(user, pass)) { 
       send(sd, MSG_530, strlen(MSG_530), 0);
       return 0;
    }
    sprintf(response, MSG_230, user);
    send(sd, response, strlen(response), 0);
    // confirm login
    return 1;
}

/**
 *  function: execute all commands (RETR|QUIT)
 *  sd: socket descriptor
 **/

void operate(int sd) {
    char op[CMDSIZE], param[PARSIZE];

    while (1) {
        op[0] = param[0] = '\0';
        // check for commands send by the client if not inform and exit


        if (strcmp(op, "RETR") == 0) {
            retr(sd, param);
        } else if (strcmp(op, "QUIT") == 0) {
            // send goodbye and close connection

            break;
        } else {
            // invalid command
            // future use
        }
    }
}


/**
 * Run with
 *         ./mysrv <SERVER_PORT>
 **/
int main (int argc, char *argv[])
{
    int md = 0, sd = 0;
    in_port_t port;
    int yes = 1;
    struct sockaddr_in srv_addr;
    struct sockaddr_storage cli_addr; // connector address information (big enough)
    socklen_t cli_size = sizeof cli_addr;

    if(argc != 2) {
        fprintf(stderr, "Usage: mysrv <server_port>");
        exit(1);
    }

    port = htons(atoi(argv[1]));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = port;
    srv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    memset(srv_addr.sin_zero, '\0', sizeof srv_addr.sin_zero);

    md = socket(AF_INET, SOCK_STREAM, 0);
    if(md == -1) {
        perror("socket");
        exit(1);
    }

    if(setsockopt(md, SOL_SOCKET, SO_REUSEADDR,&yes, sizeof yes) == -1) {
        perror("setsocketopt");
        exit(1);
    }

    if(bind(md, (struct sockaddr*) &srv_addr, sizeof srv_addr) == -1) {
        perror("bind");
        exit(1);
    }

    if(listen(md, 10) == -1){
        perror("listen");
        close(md);
        exit(1);
    }
    printf("Server waiting for connection...\n");

    while(1) {
        sd = accept(md, (struct sockaddr*) &cli_addr, &cli_size);
        handle_connection(sd, md);
    }

    return 0;
}

void handle_connection(int sd, int md) {
    char msg[] = "Hello world\n";
    if(sd == -1){
        perror("Accept");
        return;
    }

    if(!fork()){ // if process is a child
        close(md); // child does not require this
        // send message
        if(send(sd, MSG_220, strlen(MSG_220), 0) == -1) {
            close(sd);
            perror("MSG_220");
            exit(1);
        }

        if(!authenticate(sd)){
            close(sd);
            exit(1);
        }
        
        printf("Entramos para cerrar1234\n");
        //operate(sd);

        if(send(sd, MSG_221, strlen(MSG_221), 0) == -1) {
            perror("MSG_221");
            close(sd);
            exit(1);
        }

        close(sd);
        exit(0);
    }
    close(sd);
}

void remove_last_two(char *buf) {
    int len = strlen(buf);
    buf[len-1] = (buf[len-2] = '\0');
}
