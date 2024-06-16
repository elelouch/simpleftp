#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <netdb.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>


#include <netinet/in.h>

#define BUFSIZE 512
#define CMDSIZE 4
#define PARSIZE 100
#define BACKLOG_SIZE 10

#define MSG_220 "220 srvFtp version 1.0\r\n"
#define MSG_331 "331 Password required for %s\r\n"
#define MSG_230 "230 User %s logged in\r\n"
#define MSG_530 "530 Login incorrect\r\n"
#define MSG_221 "221 Goodbye\r\n"
#define MSG_550 "550 %s: no such file or directory\r\n"
#define MSG_299 "299 File %s size %ld bytes\r\n"
#define MSG_226 "226 Transfer complete\r\n"

void handle_connection(int, int);

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
    if(recv(sd, buffer, BUFSIZE, 0) == -1) {
        fprintf(stderr, "Error while receiving command\n");
        return 0;
    }

    // expunge the terminator characters from the buffer
    buffer[strcspn(buffer, "\r\n")] = 0;

    // complex parsing of the buffer
    // extract command received in operation if not set \0
    // extract parameters of the operation in param if it needed
    token = strtok(buffer, " ");
    if (token == NULL || strlen(token) < 4) {
        warn("not valid ftp command");
        return 0;
    }
    if (operation[0] == '\0') strcpy(operation, token);
    if (strcmp(operation, token)) {
        warn("Abnormal client flow: did not send %s command", operation);
        return 0;
    }
    token = strtok(NULL, " ");
    if (token != NULL) strcpy(param, token);
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
    if(send(sd, buffer, strlen(buffer), 0) == -1){
        fprintf(stderr, "Couldn't send answer to client\n");
        return 0;
    }

    return 1;
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
    file = fopen(file_path, "rw");
    if(!file) {
        send_ans(sd, MSG_550);
        return;
    }

    // send a success message with the file length
    fseek(file, 0L, SEEK_END);
    fsize = ftell(file);
    fseek(file, 0L, SEEK_SET);
    send_ans(sd, MSG_299, file_path);

    // important delay for avoid problems with buffer size
    sleep(1);

    // send the file
    while(fread(buffer, BUFSIZE, 1, file)){
        send(sd, buffer, BUFSIZE, 0);
    }
          
    // close the file

    // send a completed transfer message
}
/**
 * funcion: check valid credentials in ftpusers file
 * user: login user name
 * pass: user password
 * return: true if found or false if no
 **/
int check_credentials(char *user, char *pass) {
    FILE *file;
    char *path = "./ftpusers", *line = NULL, credentials[100];
    size_t line_size = 0;
    int found = 0;

    // make the credential string
    sprintf(credentials, "%s:%s", user, pass);

    // check if ftpusers file it's present
    if ((file = fopen(path, "r"))==NULL) {
        warn("Error opening %s", path);
        return 0;
    }

    // search for credential string
    while (getline(&line, &line_size, file) != -1) {
        strtok(line, "\n");
        if (strcmp(line, credentials) == 0) {
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
    char user[PARSIZE], pass[PARSIZE];

    // wait to receive USER action
    if(!recv_cmd(sd, "USER", user)){
        return 0;
    }

    // ask for password
    send_ans(sd, "Waiting for PASS action...\n");

    // wait to receive PASS action
    if(!recv_cmd(sd, "PASS", pass)){
        return 0;
    }
    // if credentials don't check denied login
    if(check_credentials(user, pass)) {
        send_ans(sd, MSG_530);
        return 0;
    }

    // confirm login
    return send_ans(sd, MSG_230, user);
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
        if(!recv_cmd(sd, op, op)) {
            fprintf(stderr, "Couldn't receive command\n");
            return;
        }

        if (strcmp(op, "RETR") == 0) {
            retr(sd, param);
        } else if (strcmp(op, "QUIT") == 0) {
            // send goodbye and close connection

            return;
        } else {
            // invalid command
            // furute use
        }
    }
}

/**
 * Run with
 *         ./mysrv <SERVER_PORT>
 **/
int main (int argc, char *argv[]) {
    // reserve sockets and variables space
    int msd, ssd, addrstat, yes = 1;
    socklen_t port = 0;
    struct addrinfo hints;
    struct addrinfo *p, *rp;
    struct sockaddr_storage peer_addr; // container big enough to support both ipv4 and ipv6
    socklen_t peer_len = sizeof peer_addr;

    // arguments checking
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    addrstat = getaddrinfo(NULL, argv[1], &hints, &p);
    if(addrstat != 0) {
        fprintf(stderr, "getaddrinfo string: %s\n", gai_strerror(addrstat));
        exit(EXIT_FAILURE);
    }

    for(rp = p; rp != NULL; rp = rp->ai_next) {
        msd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if(msd == -1) {
            perror("socket");
            continue;
        }

        if(setsockopt(msd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }

        if(bind(msd, rp->ai_addr, rp->ai_addrlen) == -1){
            perror("bind");
            close(msd);
        }
        
        break; // socket binded, success
    }

    freeaddrinfo(p);

    if(listen(msd, BACKLOG_SIZE) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    for(;;) {
        ssd = accept(msd, (struct sockaddr*) &peer_addr, &peer_len);
        handle_connection(ssd, msd);
    }

    return 0;
}

