#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "socketmgmt.h"

// Constants
#define DOMAINLEN 256
#define BUFSIZE 512
#define PORTLEN 6 // "65536" + '\0'
#define FTP_PORT_STR "21"
#define FTP_DATA_PORT_STR "20"
                    
// Codes
#define HELLO_CODE 220
#define DIRECTORY_CHANGED_OK_CODE 250
#define FILE_ACTION_OK_CODE 220
#define GOODBYE_CODE 220
#define WRONG_LOGIN_CODE 550
#define LOGGED_CODE 230
#define PASSWORD_REQUIRED_CODE 331
#define TRANSFER_COMPLETE_CODE 226
#define ENTERING_PASV_MODE_CODE 227
#define OPENING_BINARY_CONN_CODE 150
#define OK_CODE 200

// Messages
#define MSG_220 "220 srvFtp version 1.0\r\n"
#define MSG_331 "331 Password required for %s\r\n"
#define MSG_230 "230 User %s logged in\r\n"
#define MSG_530 "530 Login incorrect\r\n"
#define MSG_221 "221 Goodbye\r\n"
#define MSG_550 "550 %s: no such file or directory\r\n"
#define MSG_299 "299 File %s size %ld bytes\r\n"
#define MSG_226 "226 Transfer complete\r\n"

#define PROMPT "ftp> "

char usage_msg[] = "Usage: %s [OPTIONS]... SERVER_NAME SERVER_PORT\n"
                    "\t OPTIONS:\n" 
                    "\t\t -A, enable active mode\n"
                    "\t\t -v, enable verbose mode\n"
                    "\t\t -h, print this help menu\n"
                    "\n\t example: %s -Av localhost 21\n\n"
                    "Commands implemented: ls, put, get, cd, pwd\n";

struct conn_stats {
    int cmd_chnl;
    int passivemode;
    int verbose;
};

int handle_pasv(struct conn_stats * stats);

int handle_port(struct conn_stats *stats);

/* Sends CWD to the server.
 * dirname: directory to change
 * stats: state passed through function calls
 */
void cd(char *dirname, struct conn_stats *stats);

/* Parses the PASV answer and calculates the port based on the answer received. 
 * The network address of the response is ignored in this implementation. It is assumed
 * that the address is the same one that belong to the server.
 *
 * src: input string with the whole answer. E.g: "Entering Passive Mode (x,x,x,x,y,y)"
 * */
int port_from_pasvres(char *src);

char *generate_port_res(int port, char *ip_addr, char *dst);
/**
 * function: receives and analizes the answer from the server
 * sd: socket descriptor
 * text: normally NULL. If a pointer is received as parameter
 *       then it is setted as the optional part of the message.
 * return: code received in the message. The design was changed since checking for general codes
 * that starts with certain numbers (like 2xx) gives more flexibility.
 **/
int recv_msg(struct conn_stats *stats, char *text);

/**
 * function: send formmated command to the server
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
int authenticate(struct conn_stats *stats);

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
void quit(struct conn_stats *params);

/**
 * function: make all operations (get|quit)
 * sd: socket descriptor
 **/
void operate(struct conn_stats*);

/**
 * sends LIST command to server
 */
void ls(struct conn_stats*);

/* Setups passive or active data connection.
 * The opened file must be handled properly afterwards.
 * returns: data connection file descriptor, 0 if couldn't set connection
 */
int dataconn(struct conn_stats*);

/*
 * Sends STOR command.
 * filename: file to be send from the client to the server.
 * stats: state of the program
 */
void store(char *filename, struct conn_stats *stats);

/* sends PWD to server and receives msg
 * stats: connection status
 */
void pwd(struct conn_stats* stats);
