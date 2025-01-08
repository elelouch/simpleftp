#include "myftp_skel.h"

int main(int argc, char *argv[]) 
{
    int sd;
    char *network_address, *port, c;
    struct conn_stats stats;

    // arguments checking
    if(argc <= 2) {
        fprintf(stderr, "Usage: %s -[OPTIONS]... SERVER_NAME SERVER_PORT\n"
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
    // if receive hello proceed with authenticate and operate if not warning
    if(recv_msg(sd, NULL) != HELLO_CODE) {
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
    stats.verbose = 0;

    while((c = getopt(argc, argv, "A:v")) != -1) {
        switch (c) {
        case 'A':
            stats.passivemode = 0;
            break;
        case 'v':
            stats.verbose = 1;
            break;
        }
    }

    operate(&stats);

    // close socket
    close(sd);

    return 0;
}

int recv_msg(int sd, char *text) 
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
    if(recv_code / 100 == 5) {
        fprintf(stderr, "Command not recognized by server\n");
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

char *read_input() {
    char *input = malloc(BUFSIZE);
    if (fgets(input, BUFSIZE, stdin)) {
        return strtok(input, "\n");
    }
    return NULL;
}

int authenticate(int sd) 
{
    char *input = NULL, desc[100];

    if(!sd) {
        fprintf(stderr, "authenticate: invalid argument (sd)\n");
        exit(EXIT_FAILURE);
    }

    // ask for user
    printf("username: ");
    input = read_input();

    // send the command to the server
    send_msg(sd, "USER", input);
    // release memory
    free(input);
    input = NULL;

    // wait to receive password requirement and check for errors
    if(recv_msg(sd, desc) != PASSWORD_REQUIRED_CODE) {
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
    if(recv_msg(sd, desc) != LOGGED_CODE) {
        fprintf(stderr, "authenticate: Incorrect credentials: %s\n", desc);
        return 0;
    }

    return 1;
}


void get(char *file_name, struct conn_stats *stats) 
{
    char buffer[BUFSIZE];
    char c;
    int cmd_sd, code_received;
    FILE *write_file, *read_file;

    if(!file_name) {
        fprintf(stderr, "get: invalid argument (file_name)");
        exit(EXIT_FAILURE);
    }

    if(!stats) {
        fprintf(stderr, "get: invalid argument (stats)");
        exit(EXIT_FAILURE);
    }

    cmd_sd = stats -> cmd_chnl;

    // error checking missing
    // setups data channel
    read_file = dataconn(stats, "r");

    // send the RETR command to the server
    send_msg(cmd_sd, "RETR", file_name);
    
    // check for the response
    code_received = recv_msg(cmd_sd, buffer);

    if(code_received / 100 != 1) {
        fprintf(stderr, "get: Action initialized code (1xx) not received\n");
        fclose(read_file);
        return;
    }

    // parsing the write_file size from the answer received
    // sscanf(buffer, "File %*s size %d bytes", &f_size);

    write_file = fopen(file_name, "w");

    //receive the write_file
    while ((c = getc(read_file)) != EOF) {
        putc(c, write_file);
    }

    // close the files 
    fclose(write_file);
    fclose(read_file);

    // receive the OK from the server
    if(recv_msg(stats -> cmd_chnl, NULL) != TRANSFER_COMPLETE_CODE) {
        fprintf(stderr, "get: Transfer complete code not received\n");
    }
}

void quit(int sd) 
{
    // send command QUIT to the client
    send_msg(sd, "QUIT", NULL);
    // receive the answer from the server
    if(recv_msg(sd, NULL) != GOODBYE_CODE) {
        fprintf(stderr, "quit: server didn't send 221 code\n");
    }
    close(sd);
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
            quit(stats -> cmd_chnl);
            break;
        } else if (strcmp(op, "ls") == 0) {
            ls(stats);
        } else if (strcmp(op, "store") == 0) {
            store(param, stats);
        } else if (strcmp(op, "cd") == 0) {
            cd(param, stats);
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
    int port, code_received;
    struct sockaddr peer_addr;
    socklen_t peer_addrlen = sizeof(peer_addr);

    // error checking
    if(!stats || !stats -> cmd_chnl) {
        fprintf(stderr, "dataconn: invalid arguments (stats)\n");
        exit(EXIT_FAILURE);
    }

    if(!mode) {
        fprintf(stderr, "dataconn: invalid arguments (mode)\n");
        exit(EXIT_FAILURE);
    }

    ip_from_sd(stats -> cmd_chnl, ip_addr);

    if(stats -> passivemode) {
        send_msg(stats -> cmd_chnl, "PASV", NULL);

        if (recv_msg(stats -> cmd_chnl, buff) != ENTERING_PASV_MODE_CODE) {
            fprintf(stderr, "dataconn: Abnormal flow\n");
            return 0;
        }

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
                                                            
    // takes each byte of the address
    sscanf(ip_addr, "%d.%d.%d.%d", &octal_0, &octal_1, &octal_2, &octal_3);

    // rewrites the address in the PORT format
    sprintf(buff, "%d,%d,%d,%d,%d,%d", octal_0, octal_1, octal_2, octal_3, port / 256, port % 256);

    send_msg(stats -> cmd_chnl, "PORT", buff);
                                                            
    code_received = recv_msg(stats -> cmd_chnl, NULL); 

    if(code_received / 100 != 2) {
        fprintf(stderr, "dataconn: OK code not received after PORT command");
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
    char buf[PORTLEN];
    char *inside_parentheses;
    int port_lowerbits = 0;
    int port_upperbits = 0;

    if(!src || !dst) {
        fprintf(stderr, "parse_pasvres, 'src' or 'dst' not available");
        exit(EXIT_FAILURE);
    }

    strtok(src,"()");
    inside_parentheses = strtok(NULL,"()");

    sscanf(inside_parentheses, "%*d,%*d,%*d,%*d,%d,%d", &port_upperbits, &port_lowerbits);
    sprintf(buf, "%d", port_upperbits * 256 + port_lowerbits);
    strncpy(dst, buf, PORTLEN);
}

void ls (struct conn_stats *stats) 
{
    FILE *data = NULL;
    int bytes_read;
    char buffer[BUFSIZE] = {'\0'};

    if (!stats || !stats -> cmd_chnl) {
        fprintf(stderr, "ls: invalid argument (stats)\n");
        exit(EXIT_FAILURE);
    }

    data = dataconn(stats, "r");

    send_msg(stats -> cmd_chnl, "LIST", NULL);

    if(recv_msg(stats -> cmd_chnl, buffer) / 100 != 1) {
        fprintf(stderr, "file action code not received\n");
    }

    while(!feof(data)) {
        bytes_read = fread(buffer, sizeof(char), BUFSIZE, data);
        fwrite(buffer, sizeof(char), bytes_read, stdout);
    }

    fclose(data);

    if(recv_msg(stats -> cmd_chnl, NULL) != TRANSFER_COMPLETE_CODE) {
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

    code_received = recv_msg(stats -> cmd_chnl, NULL);
    if (code_received != DIRECTORY_CHANGED_OK_CODE) {
        fprintf(stderr, "directory was not changed correctly, ok code not received\n");
    }
}

void store(char *filename, struct conn_stats *stats) 
{
    FILE *data = NULL, *send_file;
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

    data = dataconn(stats, "w");

    send_file = fopen(filename, "r");

    send_msg(stats -> cmd_chnl, "STOR", filename);

    recv_msg(stats -> cmd_chnl, buffer);

    while(!feof(send_file)) {
        bytes_read = fread(buffer, sizeof(char), BUFSIZE, send_file);
        fwrite(buffer, sizeof(char), bytes_read, data);
    }

    fclose(data);

    if(recv_msg(stats -> cmd_chnl, NULL) != TRANSFER_COMPLETE_CODE) {
        fprintf(stderr, "get: transfer complete code not received\n");
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
        perror("getpeername");
        return;
    }

    af = addr.sa_family;

    if (af == AF_INET) {
        inet_ntop(af, &((struct sockaddr_in*) &addr) -> sin_addr, dst, INET_ADDRSTRLEN);
    } else {
        inet_ntop(af, &((struct sockaddr_in6*) &addr) -> sin6_addr, dst, INET6_ADDRSTRLEN);
    }
}
