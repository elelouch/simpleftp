#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <netdb.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 512
#define CMDSIZE 4
#define PARSIZE 100
#define BACKLOG_SIZE 10
#define PORT_LEN 5 + 1 // (2^16) + '\0'

#define MSG_220 "220 srvFtp version 1.0\r\n"
#define MSG_331 "331 Password required for %s\r\n"
#define MSG_230 "230 User %s logged in\r\n"
#define MSG_530 "530 Login incorrect\r\n"
#define MSG_221 "221 Goodbye\r\n"
#define MSG_550 "550 %s: no such file or directory\r\n"
#define MSG_502 "502 Command not implemented\r\n"
#define MSG_501 "501 Syntax errors in parameters or arguments\r\n"
#define MSG_299 "299 File %s size %ld bytes\r\n"
#define MSG_226 "226 Transfer complete\r\n"
#define MSG_229 "229 Entering Extended Passive Mode (|||%s|)\r\n"
#define MSG_227 "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n"
#define MSG_215 "215 UNIX Type\r\n"
#define MSG_211 "211 - RETR <path>: retrieve a file\n\
                 211 - QUIT: stop connection\r\n"

/* builds the espv response using the opened sd
 * return: -1 on error
 *          0 on success
 */
int send_pasv_ans(int cmd_chnl_sd, int file_chnl_sd); 
/* builds the espv response using the opened sd
 * return: -1 on error
 *          0 on success
 */
int send_epsv_ans(int cmd_chnl_sd, int file_chnl_sd); 

/* Opens a socket for a tcp server.
 * If service is null, then the OS chooses the port.
 * return: server socket descriptor
 */
int opensock_tcpsrv(char *address, char *service);

int pasv(int sd);

int epsv(int sd);
/* handles peer connection in the main loop
 * ssd: slave socket descriptor
 *  msd: master socket descriptor
 *  return 
 */
void handle_connection(int, int);
/**
 * function: send answer to the client
 * sd: file descriptor
 * message: formatting string in printf format
 * ...: variable arguments for economics of formats
 * return: true if not problem arise or else
 * notes: the MSG_x have preformated for these use
 **/
int send_ans(int sd, char *message, ...);

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

int recv_cmd(int sd, char *operation, char *param);
/**
 * function: RETR operation
 * sd: socket descriptor
 * file_path: name of the RETR file
 **/
void retr(int sd, char *file_path);
/**
 * funcion: check valid credentials in ftpusers file
 * user: login user name
 * pass: user password
 * return: true if found or false if no
 **/
int check_credentials(char *user, char *pass);
/**
 * function: login process management
 * sd: socket descriptor
 * return: true if login is succesfully, false if not
 **/
int authenticate(int sd);
/**
 *  function: execute all commands (RETR|QUIT)
 *  sd: socket descriptor
 **/
void operate(int sd);
/**
 * Run with
 *         ./mysrv <SERVER_PORT>
 **/
int main(int argc, char *argv[])
{
    // reserve sockets and variables space
    struct sockaddr_storage peer_addr; // container big enough to support both ipv4 and ipv6
    socklen_t peer_len = sizeof peer_addr;
    int msd = 0, ssd = 0;

    // arguments checking
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    msd = opensock_tcpsrv(NULL, "3490");

    if (listen(msd, BACKLOG_SIZE) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    for (;;) {
        ssd = accept(msd, (struct sockaddr *)&peer_addr, &peer_len);
        handle_connection(ssd, msd);
    }

    close(msd);

    return 0;
}

void handle_connection(int ssd, int msd)
{
    if (ssd == -1) {
        perror("handle_connection:socket");
        return;
    }

    if (!fork()) {
        close(msd);
        send_ans(ssd, MSG_220);
        if (!authenticate(ssd)) {
            close(ssd);
            exit(EXIT_FAILURE);
        }
        operate(ssd);
        // securing
        close(ssd);
        exit(EXIT_SUCCESS);
    }

    close(ssd);
}

int send_ans(int sd, char *message, ...)
{
    char buffer[BUFSIZE];


    va_list args;
    va_start(args, message);

    vsprintf(buffer, message, args);
    va_end(args);
    // send answer preformated and check errors
    printf("is socket: %d\n", S_ISSOCK(sd));
    printf("send_ans fd: %d\n", fcntl(sd, F_GETFD));
    if (send(sd, buffer, strlen(buffer), 0) == -1) {
        perror("send_ans:send");
        return 0;
    }

    return 1;
}

int recv_cmd(int sd, char *operation, char *param)
{
    char buffer[BUFSIZE], *token;
    int recv_s;

    // receive the command in the buffer and check for errors
    recv_s = recv(sd, buffer, BUFSIZE, 0);
    if (recv_s == -1) {
        fprintf(stderr, "Error while receiving host command\n");
        return 0;
    }

    if (recv_s == 0) {
        printf("Host performed an orderly shutdown\n");
        exit(EXIT_FAILURE);
    }

    printf("client command: %s\n", buffer);
    // expunge the terminator characters from the buffer
    buffer[strcspn(buffer, "\r\n")] = 0;

    // complex parsing of the buffer
    // extract command received in operation if not set \0
    // extract parameters of the operation in param if it needed
    token = strtok(buffer, " ");
    if (token == NULL || strlen(token) < 4){
        warn("not valid ftp command");
        return 0;
    }
    if (operation[0] == '\0')
        strcpy(operation, token);
    if (strcmp(operation, token)){
        warn("Abnormal client flow: did not send %s command", operation);
        return 0;
    }

    token = strtok(NULL, " ");
    if (token != NULL)
        strcpy(param, token);
    return 1;
}

void retr(int sd, char *file_path)
{
    FILE *file;
    int bread;
    long fsize;
    char buffer[BUFSIZE];

    if(fork()) return;

    // check if file exists if not inform error to client
    file = fopen(file_path, "r");
    if (!file) {
        send_ans(sd, MSG_550); // TODO: suggest to use LIST command
        return;
    }

    // send a success message with the file length
    fseek(file, 0L, SEEK_END);
    fsize = ftell(file);
    fseek(file, 0L, SEEK_SET);
    send_ans(sd, MSG_299, file_path, fsize);

    // important delay for avoid problems with buffer size
    sleep(1);

    // send the file
    while ((bread = fread(buffer, BUFSIZE, 1, file))) {
        send(sd, buffer, bread, 0);
    }

    if (ferror(file))
        send_ans(sd, "Error while reading %s", file_path);
    else
        send_ans(sd, MSG_226);

    // close the file
    fclose(file);
    exit(EXIT_SUCCESS);
}

int check_credentials(char *user, char *pass)
{
    FILE *file;
    char *path = "./ftpusers", *line = NULL, credentials[100];
    size_t line_size = 0;
    int found = 0;

    // make the
    sprintf(credentials, "%s:%s", user, pass);

    // check if ftpusers file it's present
    if ((file = fopen(path, "r")) == NULL) {
        warn("Error opening %s", path);
        return 0;
    }
    while (getline(&line, &line_size, file) != -1) {
        strtok(line, "\n");
        if (strcmp(line, credentials) == 0) {
            found = 1;
            break;
        }
    }
    fclose(file);
    if (line)
        free(line);
    return found;
}

int authenticate(int sd)
{
    char user[PARSIZE], pass[PARSIZE];

    if (!recv_cmd(sd, "USER", user)) {
        return 0;
    }

    send_ans(sd, MSG_331, user);

    if (!recv_cmd(sd, "PASS", pass)) {
        return 0;
    }

    if (!check_credentials(user, pass))
    {
        send_ans(sd, MSG_530);
        return 0;
    }
    return send_ans(sd, MSG_230, user);
}

void operate(int sd)
{
    char op[CMDSIZE], param[PARSIZE];
    int transfer_chnl = -1;

    for (;;) {
        op[0] = param[0] = '\0';

        if (!recv_cmd(sd, op, param))
            return;

        if (!strcmp(op, "RETR")) {
            retr(transfer_chnl, param);
        } else if (strcmp(op, "QUIT") == 0) {
            send_ans(sd, MSG_221);
            close(sd);
            exit(EXIT_SUCCESS);
        }
        //else if (strcmp(op, "EPSV") == 0) {
        //    transfer_chnl = epsv(sd);
        //}
        else if (strcmp(op, "SYST") == 0) {
            send_ans(sd, MSG_215);
        } else if (strcmp(op, "FEAT") == 0) {
            send_ans(sd, MSG_211);
        } else if (strcmp(op, "PASV") == 0)  {
            transfer_chnl = pasv(sd);
        } else {
            send_ans(sd, MSG_502);
        }
    }
}

int epsv(int cmd_chnl)
{
    int file_chnl = opensock_tcpsrv("127.0.0.1", NULL);

    if(file_chnl == -1) {
        send_ans(cmd_chnl, MSG_502);
        return -1;
    }

    send_epsv_ans(cmd_chnl, file_chnl); // Error checking 
    return file_chnl;
}

int opensock_tcpsrv(char *address, char *service) 
{
    int sd, addrstat, yes = 1;
    struct addrinfo hints;
    struct addrinfo *p, *rp;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* TCP connection */
    hints.ai_flags = AI_PASSIVE;     /* For wildcard IP address */

    addrstat = getaddrinfo(address, service, &hints, &p);
    if (addrstat != 0)
    {
        fprintf(stderr, "getaddrinfo string: %s\n", gai_strerror(addrstat));
        return -1;
    }

    // bind the first socket found
    for (rp = p; rp != NULL; rp = rp->ai_next){
        sd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sd == -1) {
            perror("opensock_tscpsrv:socket");
            continue;
        }

        if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            return -1;
        }

        if (bind(sd, rp->ai_addr, rp->ai_addrlen) == -1) {
            perror("bind");
            close(sd);
        }

        break; // socket binded, success
    }

    if (!rp) {
        fprintf(stderr, "Server failed to bind\n");
        return -1;
    }

    freeaddrinfo(p);

    // and return socket
    return sd;
}


int send_epsv_ans(int cmd_chnl_sd, int file_chnl_sd) {
    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    char port_str[PORT_LEN];

    if(getsockname(file_chnl_sd, (struct sockaddr*) &addr, &len) == -1)
        return -1;

    if(addr.ss_family == AF_INET) {
        sprintf(port_str, "%hu", ((struct sockaddr_in*) &addr)->sin_port);
    } else { 
        sprintf(port_str, "%hu", ((struct sockaddr_in6*) &addr)->sin6_port);
    }

    send_ans(cmd_chnl_sd, MSG_229, port_str);
    return 0;
}

int send_pasv_ans(int cmd_chnl_sd, int file_chnl_sd) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int a, b, c, d, port;
    char ipv4_str[INET_ADDRSTRLEN];

    if(getsockname(file_chnl_sd, (struct sockaddr*) &addr, &len) == -1)
        return -1;


    inet_ntop(AF_INET, &addr, ipv4_str, INET_ADDRSTRLEN);

    port = htons(addr.sin_port);

    sscanf(ipv4_str,"%d.%d.%d.%d", &a, &b, &c, &d);


    send_ans(cmd_chnl_sd, MSG_227, a, b, c, d, port / 256, port % 256);

    return 0;
}

int pasv(int cmd_chnl)
{
    int file_chnl = opensock_tcpsrv("127.0.0.1", NULL);

    if(file_chnl == -1) {
        send_ans(cmd_chnl, MSG_502);
        fprintf(stderr, "Error while opening sockets\n");
        return -1;
    }

    send_pasv_ans(cmd_chnl, file_chnl); // Error checking 
    return file_chnl;
}
