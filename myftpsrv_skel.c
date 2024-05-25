#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "tcp_utils.h"
#include <stdbool.h>
#include <stdarg.h>
#include <err.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

// #include "tcp_utils.h"

#define BUFSIZE 512
#define CMDSIZE 4
#define PARSIZE 100
#define BACKLOG_SIZE 128

#define MSG_220 "220 srvFtp version 1.0\r\n"
#define MSG_331 "331 Password required for %s\r\n"
#define MSG_230 "230 User %s logged in\r\n"
#define MSG_530 "530 Login incorrect\r\n"
#define MSG_221 "221 Goodbye\r\n"
#define MSG_550 "550 %s: no such file or directory\r\n"
#define MSG_299 "299 File %s size %ld bytes\r\n"
#define MSG_226 "226 Transfer complete\r\n"

void handle_conn(int, int);
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
bool recv_cmd(int sd, char *operation, char *param) {
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
        warn("not valid ftp command");
        return false;
    } 
    if (operation[0] == '\0') strcpy(operation, token);
    if (strcmp(operation, token)) {
        warn("abnormal client flow: did not send %s command", operation);
        return false;
    }
    token = strtok(NULL, " ");
    if (token) strcpy(param, token);
    return true;
}

/**
 * function: send answer to the client
 * sd: file descriptor
 * message: formatting string in printf format
 * ...: variable arguments for economics of formats
 * return: true if not problem arise or else
 * notes: the MSG_x have preformated for these use
 **/
bool send_ans(int sd, char *message, ...){
    char buffer[BUFSIZE];

    va_list args;
    va_start(args, message);

    vsprintf(buffer, message, args);
    va_end(args);
    // send answer preformated and check errors

    return false;
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
bool check_credentials(char *user, char *pass) {
    FILE *file;
    char *path = "./ftpusers", *line = NULL, credentials[100];
    size_t line_size = 0;
    bool found = false;

    // make the credential string
    sprintf(credentials, "%s:%s", user, pass);

    // check if ftpusers file it's present
    file = fopen(path, "r");
    if (!file) {
        warn("Error opening %s", path);
        return false;
    }

    // search for credential string
    while (getline(&line, &line_size, file) != -1) {
        strtok(line, "\n");
        if (!strcmp(line, credentials)) {
            found = true;
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
bool authenticate(int sd) {
    char user[PARSIZE], pass[PARSIZE];

    // wait to receive USER action


    // ask for password

    // wait to receive PASS action

    // if credentials don't check denied login

    // confirm login
    return 0;
}

/**
 *  function: execute all commands (RETR|QUIT)
 *  sd: socket descriptor
 **/

void operate(int sd) {
    char op[CMDSIZE], param[PARSIZE];

    while (true) {
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


void sigchld_handler(int sig)
{
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

void* get_in_addr(struct sockaddr* addr) {
    if(addr->sa_family == AF_INET) {
        return &((struct sockaddr_in*) addr)->sin_addr;
    }
    return &((struct sockaddr_in6*) addr)->sin6_addr;
}

/**
 * Run with
 *         ./mysrv <SERVER_PORT>
 **/
int main (int argc, char *argv[])
{
    int master_sd = 0, slave_sd = 0, yes = 1, rv = 0, pid = 0;
    char* port = 0;
    struct sigaction sa;
    struct sockaddr_storage their_addr; // connector address information (big enough)
    socklen_t slave_addrlen = 0;
    struct addrinfo *servinfo, *p, hints;
    socklen_t sin_size;
    char dst[INET6_ADDRSTRLEN];
    char message[] = "Hello, world!\n";
    int message_length = sizeof message / sizeof (char);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;

    if(argc != 2) {
        fprintf(stderr, "Usage mysrv <server_port>");
        return 1;
    }
    port = argv[1];

    if((rv = getaddrinfo(NULL,port, &hints, &servinfo)) != 0){
        fprintf(stderr,"Could not get any address, getaddrinfo: %s\n",gai_strerror(rv));
        return 1;
    }
    for((p = servinfo); p != NULL; p = p->ai_next){
        master_sd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if(master_sd == -1){
            perror("socket");
            continue;
        }

        if(setsockopt(master_sd, SOL_SOCKET, SO_REUSEADDR, &yes, p->ai_addrlen) == -1){
            perror("setsockopt");
            exit(1);
        }

        if(bind(master_sd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("bind"); // we'll try with the next one
            close(master_sd);
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo);
    if(p == NULL) {
        fprintf(stderr, "Couldn't bind any address\n");
        exit(1);
    }

    if(listen(master_sd, 10) == -1){
        perror("listen");
        close(master_sd);
        exit(1);
    }

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask); // empties all the signals of the mask
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGCHLD, &sa, NULL) == -1){
        perror("sigaction");
        exit(1);
    }


    printf("Server waiting for connection ...\n");
    for(;;) {
        sin_size = sizeof their_addr;
        slave_sd = accept(master_sd, (struct sockaddr*) &their_addr, &sin_size);
        if(slave_sd == -1) {
            perror("accept");
            close(master_sd);
            exit(1);
        }
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr*) &their_addr), dst, sin_size);
        printf("received connection from address %s\n", dst);

        if(!fork()) {
            close(master_sd); // closes mastersd just for child
            if(send(slave_sd, message, message_length, 0) == -1) {
                perror("send");
            }
            close(slave_sd);
            exit(0);
        }
        close(slave_sd);
    }

    
    return 0;
}

void handle_conn(int slave_sd, int master_sd) {
    char hello[] = "hello";
    if(slave_sd == -1)
        return;

    // send hello
    if(!fork()){
        close(master_sd);
        if(send(slave_sd, "Hello world", 13, 0) == -1) {
            perror("send hello");
            exit(1);
        }

        //if(!authenticate(slave_sd))
        //    exit(1);
        //
        //operate(slave_sd);
        exit(0);
    }
}
