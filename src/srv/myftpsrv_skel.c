#include "myftpsrv_skel.h"
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    // reserve sockets and variables space
    struct sockaddr_storage peer_addr; // container big enough to support both ipv4 and ipv6
    socklen_t peer_len = sizeof peer_addr;
    char s[INET6_ADDRSTRLEN] = {'\0'};
    int sd = 0, peer_sd = 0;
    struct sockaddr_in6 *in6_addr;
    struct sockaddr_in *in_addr;

    // arguments checking
    if (argc != 2) {
        fprintf(stderr, "Usage: %s LISTEN_PORT\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    sd = tcp_listen(argv[1], BACKLOG_SIZE);

    if(sd == -1) exit(EXIT_FAILURE);

    printf("Listening...\n");

    while (1) {
        peer_sd = accept(sd, (struct sockaddr *)&peer_addr, &peer_len);

        if(peer_sd == -1){
            fprintf(stderr, "Error while accepting connection.\n");
            continue;
        }

        if(peer_addr.ss_family == AF_INET6) {
            in6_addr = (struct sockaddr_in6*) &peer_addr;
            inet_ntop(in6_addr->sin6_family, &in6_addr -> sin6_addr, s, sizeof s);
            printf("Peer IP address : %s\n", s);
            printf("Peer port       : %d\n", ntohs(in6_addr->sin6_port));
        } else {
            printf("Peer IP address : %s\n", s);
            in_addr = (struct sockaddr_in*) &peer_addr;
            inet_ntop(in_addr->sin_family, &in_addr -> sin_addr, s, sizeof s);
            printf("Peer IP address : %s\n", s);
            printf("Peer port       : %d\n", ntohs(in_addr->sin_port));
        }

        handle_connection(peer_sd);
    }

    close(sd);

    return 0;
}

void handle_connection(int ssd)
{
    if (ssd == -1 || !ssd) {
        fprintf(stderr, "slave socked descriptor not valid\n");
        return;
    }

    if (!fork()) {
        // sess_stats is now part of the children address space
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
    if (send(sd, buffer, strlen(buffer), 0) == -1) {
        perror("send_ans:send");
        return 0;
    }

    return 1;
}

int recv_cmd(int sd, char *operation, char *param)
{
    char buffer[BUFSIZE] = {'\0'}, *token;
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

    // expunge the terminator characters from the buffer
    buffer[strcspn(buffer, "\r\n")] = 0;
    printf("Received from client: %s\n", buffer);

    // complex parsing of the buffer
    // extract command received in operation if not set \0
    // extract parameters of the operation in param if it needed
    token = strtok(buffer, " ");
    if (!token){
        warn("not valid ftp command");
        return 0;
    }

    if (operation[0] == '\0') strcpy(operation, token);

    if (strcmp(operation, token)){
        warn("Abnormal client flow: did not send %s command\n", operation);
        return 0;
    }

    token = strtok(NULL, " ");

    if (token != NULL)
        strcpy(param, token);
    return 1;
}

void retr(struct sess_stats *stats, char *file_path)
{
    FILE *file;
    int bread;
    long fsize;
    char buffer[BUFSIZE];
    char full_file_path[PATH_MAX] = {'\0'};

    if(!stats) {
        fprintf(stderr, "retr:Invalid arguments, cmd or data channel not available\n");
        return;
    }

    // check if file exists if not inform error to client
    getcwd(full_file_path, PATH_MAX);
    strcat(full_file_path, "/files");
    strcat(full_file_path, "/");
    strcat(full_file_path, file_path);

    file = fopen(full_file_path, "r");
    if (!file) {
        send_ans(stats -> cmd_chnl, MSG_550); // TODO: suggest to use LIST command
        return;
    }

    // send a success message with the file length
    fseek(file, 0L, SEEK_END);
    fsize = ftell(file);
    fseek(file, 0L, SEEK_SET);
    // send_ans(cmd_chnl, MSG_299, file_path, fsize);
    send_ans(stats -> cmd_chnl, MSG_150, file_path, fsize);

    // important delay to avoid problems with buffer size
    sleep(1);

    // send the file
    while ((bread = fread(buffer, sizeof(char), BUFSIZE, file))) 
        write(stats -> data_chnl, buffer, BUFSIZE);

    if (ferror(file))
        send_ans(stats -> cmd_chnl, "Error while reading %s", file_path);
    else
        send_ans(stats -> cmd_chnl, MSG_226);

    // close the file
    close(stats -> data_chnl);
    stats -> data_chnl = 0;
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

    if (!check_credentials(user, pass)) {
        send_ans(sd, MSG_530);
        return 0;
    }
    return send_ans(sd, MSG_230, user);
}

void operate(int sd)
{
    char op[CMDSIZE], param[PARSIZE];
    struct sess_stats stats;
    socklen_t len = sizeof stats.cmd_sau;

    memset(&stats, '\0', len);

    if(!sd) {
        fprintf(stderr,"operate: socket not valid\n");
        exit(EXIT_FAILURE);
    }

    stats.cmd_chnl = sd;
    stats.root_dir = "./files";


    if(getsockname(sd, (struct sockaddr*) &stats.cmd_sau, &len) == -1) {
        perror("getsockname");
        exit(EXIT_FAILURE);
    }
    
    chdir(stats.root_dir);

    while (1) {
        op[0] = param[0] = '\0';

        if (!recv_cmd(sd, op, param))
            return;

        if (strcmp(op, "RETR") == 0) {
            retr(&stats, param);
        } else if (strcmp(op, "QUIT") == 0) {
            send_ans(sd, MSG_221);
            close(sd);
            exit(EXIT_SUCCESS);
        } else if (strcmp(op, "LIST") == 0) {
            ls(&stats);
        } else if (strcmp(op, "SYST") == 0) {
            send_ans(sd, MSG_215);
        } else if (strcmp(op, "FEAT") == 0) {
            send_ans(sd, MSG_211);
        } else if (strcmp(op, "PASV") == 0)  { // server listens for a connection on a certain port
            pasv(&stats);
        } else if (strcmp(op, "CWD") == 0) {
            cd(&stats);
        } else {
            send_ans(sd, MSG_502);
        }
    }
}

void ls(struct sess_stats *stats) 
{
    FILE *popen_res = NULL;
    char buffer[BUFSIZE], c;
    int i = 0;

    if (!stats) {
        fprintf(stderr, "ls: stats not valid\n");
        return;
    }

    popen_res = popen("ls -ln", "r");

    if(!popen_res) {
        perror("popen");
        return;
    }

    send_ans(stats->cmd_chnl, MSG_125);

    while((c = fgetc(popen_res)) != EOF) {
        if(c == '\n') {
                buffer[i++] = '\r';
        }
        buffer[i++] = c;
        if(i > BUFSIZE) {
                write(stats->data_chnl, buffer, BUFSIZE);
                i = 0;
        }
    }
    buffer[i] = EOF;
    write(stats -> data_chnl, buffer, i);

    pclose(popen_res);
    close(stats -> data_chnl);
    stats -> data_chnl = 0;

    send_ans(stats -> cmd_chnl, MSG_226);
}

int pasv(struct sess_stats *stats)
{
    int a,b,c,d;
    char ip_addr[INET6_ADDRSTRLEN] = {'\0'};
    int data_chnl, port;
    struct sockaddr_in data_sa;
    struct sockaddr_in *cmd_sa;
    socklen_t len = sizeof(data_sa);

    if(!stats) {
        fprintf(stderr, "pasv: cmd_chnl socket not valid");
        return -1;
    }

    cmd_sa = &stats -> cmd_sau.u.sa4;

    data_chnl = tcp_listen("0", 1);

    if(data_chnl == -1) {
        send_ans(stats -> cmd_chnl, MSG_502);
        fprintf(stderr, "Error while opening sockets\n");
        return -1;
    }

    if(getsockname(data_chnl, (struct sockaddr*)&data_sa, &len) == -1){
        perror("getsockname");
        return -1;
    }

    if(!inet_ntop(cmd_sa -> sin_family, &cmd_sa->sin_addr, ip_addr, sizeof ip_addr)){
        perror("pasv:inet_ntop");
        return -1;
    }

    port = ntohs(data_sa.sin_port);

    sscanf(ip_addr,"%d.%d.%d.%d", &a, &b, &c, &d);

    send_ans(stats -> cmd_chnl, MSG_227, a, b, c, d, port / 256, port % 256);

    data_chnl = accept(data_chnl, (struct sockaddr *)&data_sa, &len);

    if (data_chnl == -1) {
        perror("accept");
        return -1;
    }

    stats -> data_chnl = data_chnl;
    
    return data_chnl;
}

void pwd(int sd) 
{
    char buffer[PATH_MAX];
    if(getcwd(buffer, PATH_MAX)) {
        send_ans(sd, MSG_257, buffer);
        return;
    }
    send_ans(sd, MSG_550, buffer);
}
