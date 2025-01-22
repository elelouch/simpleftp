#include "myftp_skel.h"
#include "socketmgmt.h"
#include <stdio.h>
#include <sys/socket.h>

int main(int argc, char *argv[]) 
{
    int sd;
    char *network_address, *port, c;
    struct conn_stats stats;
    // arguments checking
    if(argc <= 2) {
        fprintf(stderr, "Usage: %s [OPTIONS]... SERVER_NAME SERVER_PORT\n"
                "\t OPTIONS:\n" 
                "\t\t -A, enable active mode\n"
                "\t\t -v, enable verbose mode\n"
                "\n\t example: %s -Av localhost 21 \n", argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }

    network_address = argv[argc - 2];
    port = argv[argc - 1];

    sd = tcp_connection(network_address, port);

    if(!sd) {
        fprintf(stderr, "main: Couldn't connect to server, name or port might be wrong\n");
        exit(EXIT_FAILURE);
    }

    stats.passivemode = 1;
    stats.cmd_chnl = sd;
    stats.verbose = 0;

    while((c = getopt(argc, argv, "Av")) != -1) {
        switch (c) {
        case 'A':
            stats.passivemode = 0;
            break;
        case 'v':
            stats.verbose = 1;
            break;
        }
    }

    // if receive hello proceed with authenticate and operate if not warning
    if(recv_msg(&stats, NULL) != HELLO_CODE) {
        fprintf(stderr, "main: Hello message not received, quiting client...\n");
        close(sd);
        exit(EXIT_FAILURE);
    }

    if(!authenticate(&stats)) {
        close(sd);
        exit(EXIT_FAILURE);
    }

    operate(&stats);

    // close socket
    close(sd);

    return 0;
}

int recv_msg(struct conn_stats *stats, char *text) 
{
    char buffer[BUFSIZE], message[BUFSIZE];
    int recv_s, recv_code;

    // receive the answer
    recv_s = recv(stats -> cmd_chnl, buffer, BUFSIZE, 0);
    // error checking
    if (recv_s == -1) 
        perror("recv");

    if (recv_s == 0) {
        close(stats -> cmd_chnl);
        fprintf(stderr, "recv_msg: Connection closed by host\n");
        exit(EXIT_FAILURE);
    }

    // parsing the code and message receive from the answer
    sscanf(buffer, "%d %[^\r\n]\r\n", &recv_code, message);

    if(stats -> verbose) {
        printf("%d %s\n", recv_code, message);
    }

    // optional copy of parameters
    if(recv_code / 100 == 5) {
        fprintf(stderr, "Command not recognized by server. Received: %d\n", recv_code);
    }
    if(text) strcpy(text, message);
    return recv_code;
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

char *read_input() 
{
    char *input = malloc(BUFSIZE);
    if (fgets(input, BUFSIZE, stdin)) {
        return strtok(input, "\n");
    }
    return NULL;
}

int authenticate(struct conn_stats *stats) 
{
    char *input = NULL, desc[100];

    if(!stats -> cmd_chnl) {
        fprintf(stderr, "authenticate: invalid argument (sd)\n");
        exit(EXIT_FAILURE);
    }

    // ask for user
    printf("username: ");
    input = read_input();

    // send the command to the server
    send_msg(stats -> cmd_chnl, "USER", input);
    // release memory
    free(input);
    input = NULL;

    // wait to receive password requirement and check for errors
    if(recv_msg(stats, desc) != PASSWORD_REQUIRED_CODE) {
        fprintf(stderr, "authenticate: Abnormal flow: %s\n", desc);
        return 0;
    }

    // ask for password
    printf("password: ");
    input = read_input();

    // send the command to the server
    send_msg(stats -> cmd_chnl, "PASS", input);

    // release memory
    free(input);

    // wait for the answer, process it and check for errors
    if(recv_msg(stats, desc) != LOGGED_CODE) {
        fprintf(stderr, "authenticate: Incorrect credentials: %s\n", desc);
        return 0;
    }

    return 1;
}


void get(char *file_name, struct conn_stats *stats) 
{
    char buffer[BUFSIZE];
    int cmd_sd, data_sd, code_received, bread, f_size;
    FILE *write_file;

    if(!file_name) {
        fprintf(stderr, "get: invalid argument (file_name)");
        exit(EXIT_FAILURE);
    }

    if(!stats) {
        fprintf(stderr, "get: invalid argument (stats)");
        exit(EXIT_FAILURE);
    }

    cmd_sd = stats -> cmd_chnl;

    data_sd = dataconn(stats);

    if(data_sd == -1) {
        fprintf(stderr, "can't connect data channel\n");
        return;
    }
    // send the RETR command to the server
    send_msg(cmd_sd, "RETR", file_name);
    
    // check for the response
    code_received = recv_msg(stats, buffer);

    if(code_received / 100 != 1 ) {
        if(code_received != 299) {
            fprintf(stderr, "get: Action initialized code (1xx) not received\n");
            close(data_sd);
            return;
        }
        // parsing the write_file size from the answer received
        sscanf(buffer, "File %*s size %d bytes", &f_size);
    }

    write_file = fopen(file_name, "w");

    while ((bread = read(data_sd, buffer, BUFSIZE)) > 0) {
        fwrite(buffer, bread, sizeof(char), write_file);
    }

    // close the files 
    close(data_sd);
    fclose(write_file);

    // receive the OK from the server
    if(recv_msg(stats, NULL) != TRANSFER_COMPLETE_CODE) {
        fprintf(stderr, "get: Transfer complete code not received\n");
    }
}

void quit(struct conn_stats *stats) 
{
    send_msg(stats -> cmd_chnl, "QUIT", NULL);

    if(recv_msg(stats, NULL) != GOODBYE_CODE)
        fprintf(stderr, "quit: server didn't send 221 code\n");

    close(stats -> cmd_chnl);
    stats -> cmd_chnl = 0;
    exit(EXIT_SUCCESS);
}

void operate(struct conn_stats *stats) 
{
    char *input, *op, *param;

    if(!stats) {
        fprintf(stderr, "operate: invalid arguments (stats)\n");
        exit(EXIT_FAILURE);
    }

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
            quit(stats);
            break;
        } else if (strcmp(op, "ls") == 0) {
            ls(stats);
        } else if (strcmp(op, "store") == 0) {
            store(param, stats);
        } else if (strcmp(op, "cd") == 0) {
            cd(param, stats);
        } else if (strcmp(op, "pwd") == 0) {
            pwd(stats);
        } else {
            // new operations in the future
            printf("TODO: unexpected command\n");
        }
        free(input);
    }
    free(input);
}

int dataconn(struct conn_stats *stats) 
{
    if(stats -> passivemode) {
        return handle_pasv(stats);
    }
    return handle_port(stats);
}

int handle_pasv(struct conn_stats *stats)
{
    char buff[BUFSIZE] = {'\0'},
         ip_addr[INET6_ADDRSTRLEN] = {'\0'},
         port_str[PORTLEN] = {'\0'};
    struct sockaddr_in sa4;
    socklen_t len = sizeof sa4;

    if(!stats || !stats -> cmd_chnl) {
        fprintf(stderr, "dataconn: invalid arguments (stats)\n");
        exit(EXIT_FAILURE);
    }

    if(getsockname(stats -> cmd_chnl, (struct sockaddr*)&sa4, &len) == -1){
        perror("getsockname");
        return -1;
    }
        
    if(!inet_ntop(sa4.sin_family, &sa4.sin_addr, ip_addr, sizeof ip_addr)){
        perror("dataconn:inet_ntop");
        return -1;
    }

    send_msg(stats -> cmd_chnl, "PASV", NULL);

    if (recv_msg(stats, buff) != ENTERING_PASV_MODE_CODE) {
        fprintf(stderr, "dataconn: Abnormal flow\n");
        return -1;
    }

    sprintf(port_str, "%d", port_from_pasvres(buff));

    return tcp_connection(ip_addr, port_str);
}

int handle_port(struct conn_stats *stats)
{
    char buff[BUFSIZE] = {'\0'}, ip_addr[INET6_ADDRSTRLEN] = {'\0'};
    struct sockaddr_in sa4;
    socklen_t len = sizeof sa4;
    int data_sd = 0, port = 0;

    if(!stats || !stats -> cmd_chnl) {
        fprintf(stderr, "dataconn: invalid arguments (stats)\n");
        exit(EXIT_FAILURE);
    }

    data_sd = tcp_listen("0", 1);

    if(!data_sd) {
        fprintf(stderr, "dataconn: couldn't listen to tcp socket\n");
        return -1;
    }

    if(getsockname(data_sd, (struct sockaddr *) &sa4, &len)) {
        perror("getsockname");
        return -1;
    };

    port = ntohs(sa4.sin_port);

    send_msg(stats -> cmd_chnl, "PORT", generate_port_res(port, ip_addr, buff));

    if(recv_msg(stats, NULL) / 100 != 2) {
        fprintf(stderr, "dataconn: OK code not received after PORT command\n");
        close(data_sd);
        return -1;
    }

    return accept(data_sd, (struct sockaddr *) &sa4, &len);
}

char *generate_port_res(int port, char *ip_addr, char *dst)
{
    int octal_0, octal_1, octal_2, octal_3;
    sscanf(ip_addr, "%d.%d.%d.%d", &octal_0, &octal_1, &octal_2, &octal_3);
    sprintf(dst, "%d,%d,%d,%d,%d,%d", octal_0, octal_1, octal_2, octal_3, port / 256, port % 256);
    return dst;
}

int port_from_pasvres(char *pasvres) 
{
    char *inside_parentheses;
    int port_lowerbits = 0;
    int port_upperbits = 0;

    if(!pasvres) {
        fprintf(stderr, "pasvres not valid");
        exit(EXIT_FAILURE);
    }

    strtok(pasvres,"()");
    inside_parentheses = strtok(NULL,"()");

    sscanf(inside_parentheses, "%*d,%*d,%*d,%*d,%d,%d", &port_upperbits, &port_lowerbits);
    return 256 * port_upperbits + port_lowerbits;
}

void ls(struct conn_stats *stats) 
{
    int data_sd = 0, bread = 0;
    char buffer[BUFSIZE] = {'\0'};

    if (!stats || !stats -> cmd_chnl) {
        fprintf(stderr, "ls: invalid argument (stats)\n");
        exit(EXIT_FAILURE);
    }

    data_sd = dataconn(stats);

    if(data_sd == -1) {
        fprintf(stderr, "can't connect data channel\n");
        return;
    }

    send_msg(stats -> cmd_chnl, "LIST", NULL);

    if(recv_msg(stats, buffer) / 100 != 1) {
        fprintf(stderr, "file action code not received\n");
    }

    while((bread = read(data_sd, buffer, BUFSIZE)) > 0) {
        fwrite(buffer, sizeof(char), bread, stdout);
    }

    close(data_sd);

    if(recv_msg(stats, NULL) != TRANSFER_COMPLETE_CODE) {
        fprintf(stderr, "get: Transfer complete code not received\n");
    }

}

void cd(char *dirname, struct conn_stats *stats) 
{
    int code_received = 0;

    if (!dirname) {
        fprintf(stderr, "cd: invalid argument (dirname)\n");
        exit(EXIT_FAILURE);
    }

    if (!stats || !stats -> cmd_chnl) {
        fprintf(stderr, "cd: invalid argument (stats)\n");
        exit(EXIT_FAILURE);
    }

    send_msg(stats -> cmd_chnl, "CWD", dirname);

    code_received = recv_msg(stats, NULL);

    if (code_received != DIRECTORY_CHANGED_OK_CODE) {
        fprintf(stderr, "Directory was not changed successfully. "
                "Ok code not received\n");
    }
}

void store(char *filename, struct conn_stats *stats) 
{
    int data_sd = 0;
    FILE *send_file = NULL;
    char buffer[BUFSIZE] = {'\0'};
    int bytes_read = 0;

    if (!filename) {
        fprintf(stderr, "store: invalid argument (filename)\n");
        return;
    }

    if (!stats || !stats -> cmd_chnl) {
        fprintf(stderr, "store: invalid argument (stats)\n");
        return;
    }

    data_sd = dataconn(stats);

    if(data_sd == -1) {
        fprintf(stderr, "can't connect data channel\n");
        return;
    }

    send_file = fopen(filename, "r");

    send_msg(stats -> cmd_chnl, "STOR", filename);

    recv_msg(stats, buffer);

    while((bytes_read = fread(buffer, sizeof(char), BUFSIZE, send_file)) > 0) {
        write(data_sd, buffer, BUFSIZE);
    }

    close(data_sd);

    if(recv_msg(stats, NULL) != TRANSFER_COMPLETE_CODE) {
        fprintf(stderr, "get: transfer complete code not received\n");
    }
}
 
void pwd(struct conn_stats *stats) 
{
    char buffer[BUFSIZE] = {'\0'};
    int code_received = 0;

    if (!stats || !stats -> cmd_chnl) {
        fprintf(stderr, "pwd: invalid argument (stats)\n");
        return;
    }

    send_msg(stats -> cmd_chnl, "PWD", NULL);

    code_received = recv_msg(stats, buffer);

    if(!code_received) {
        fprintf(stderr, "pwd: code not received\n");
    }

    printf("%s\n", buffer);
}
