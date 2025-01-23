#include <linux/limits.h>
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
#include <limits.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "socketmgmt.h"

#define BUFSIZE 512
#define CMDSIZE 4
#define PARSIZE 100
#define BACKLOG_SIZE 10
#define PORTLEN 5 + 1 // (2^16 = 65536, 5 caracteres maximo) + '\0'

#define MSG_150 "Opening BINARY mode data connection for %s (%d bytes).\r\n"
#define MSG_125 "125 Data connection open, starting transfer\r\n"
#define MSG_200 "200 %s Command OK\r\n"
#define MSG_211 "211 - RETR <path>: retrieve a file\r\n"
#define MSG_215 "215 UNIX Type\r\n"
#define MSG_220 "220 srvFtp version 1.0\r\n"
#define MSG_230 "230 User %s logged in\r\n"
#define MSG_221 "221 Goodbye\r\n"
#define MSG_226 "226 Transfer complete\r\n"
#define MSG_250 "250 Directory successfully changed\r\n"
#define MSG_227 "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n"
#define MSG_229 "229 Entering Extended Passive Mode (|||%s|)\r\n"
#define MSG_257 "257 \"%s\" is the current directory\r\n"
#define MSG_299 "299 File %s size %ld bytes\r\n"
#define MSG_331 "331 Password required for %s\r\n"
#define MSG_501 "501 Syntax errors in parameters or arguments\r\n"
#define MSG_502 "502 Command not implemented\r\n"
#define MSG_530 "530 Login incorrect\r\n"
#define MSG_550 "550 %s: no such file or directory\r\n"

struct sess_stats {
    int cmd_chnl;
    int data_chnl;
    struct sockaddr_util cmd_sau;
};

/* executes retr concurrently using sd as file destination
 */
/* builds the pasv response using the opened sd
 * return: -1 on error
 *          0 on success
 */
int send_pasv_ans(int cmd_chnl_sd, int file_chnl_sd); 

/* Client asks to the server to listen to certain port
 * sd: cmd, command channel
 * data_chnl: data channel, if data_chnl is NULL, 
 * proceeds to open a new data channel.
 * */
int handle_pasv(struct sess_stats *stats);

/* handles peer connection in the main loop
 * ssd: slave socket descriptor
 */
void handle_connection(int sd);

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
void retr(struct sess_stats *stats, char *file_path);

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
 * Send ls response
 * cmd_chnl: command channel
 * data_chnl: data channel
 */
void ls(struct sess_stats *stats);

void cd(struct sess_stats *stats, char *dir);

int handle_port(struct sess_stats *stats, char *param);
