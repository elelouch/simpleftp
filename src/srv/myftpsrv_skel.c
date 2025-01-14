#include "myftpsrv_skel.h"
#include "socketmgmt.h"
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    // reserve sockets and variables space
    struct sockaddr_storage peer_addr; // container big enough to support both ipv4 and ipv6
    socklen_t peer_len = sizeof peer_addr;
    int sd = 0, peer_sd = 0;

    // arguments checking
    if (argc != 2) {
        fprintf(stderr, "Usage: %s LISTEN_PORT\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    sd = tcp_listen(argv[1], BACKLOG_SIZE);

    if(!sd) exit(EXIT_FAILURE);

    while (1) {
        peer_sd = accept(sd, (struct sockaddr *)&peer_addr, &peer_len);
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

    if (operation[0] == '\0') strcpy(operation, token);

    if (strcmp(operation, token)){
        warn("Abnormal client flow: did not send %s command", operation);
        return 0;
    }

    token = strtok(NULL, " ");

    if (token != NULL)
        strcpy(param, token);
    return 1;
}

void retr(int cmd_chnl, FILE *data_chnl, char *file_path)
{
    FILE *file;
    int bread;
    long fsize;
    char buffer[BUFSIZE];

    if(!cmd_chnl || !data_chnl) {
        fprintf(stderr, "retr:Invalid arguments\n");
        return;
    }

    // check if file exists if not inform error to client
    file = fopen(file_path, "r");
    if (!file) {
        send_ans(cmd_chnl, MSG_550); // TODO: suggest to use LIST command
        return;
    }

    // send a success message with the file length
    fseek(file, 0L, SEEK_END);
    fsize = ftell(file);
    fseek(file, 0L, SEEK_SET);
    send_ans(cmd_chnl, MSG_299, file_path, fsize);

    // important delay to avoid problems with buffer size
    sleep(1);

    // send the file
    while ((bread = fread(buffer, sizeof(char), BUFSIZE, file))) 
        fwrite(buffer, sizeof(char), bread, data_chnl);

    if (ferror(file))
        send_ans(cmd_chnl, "Error while reading %s", file_path);
    else
        send_ans(cmd_chnl, MSG_226);

    // close the file
    fclose(data_chnl);
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

// active -> client establishes the command channel, server is responsible for the data channel.
// pasive -> client establishes both channels.
void operate(int sd)
{
    char op[CMDSIZE], param[PARSIZE];
    FILE *data_chnl = NULL;

    while (1) {
        op[0] = param[0] = '\0';

        if (!recv_cmd(sd, op, param))
            return;

        if (strcmp(op, "RETR") == 0 && data_chnl) {
            retr(sd, data_chnl, param);
        } else if (strcmp(op, "QUIT") == 0) {
            send_ans(sd, MSG_221);
            close(sd);
            exit(EXIT_SUCCESS);
        } else if (strcmp(op, "SYST") == 0) {
            send_ans(sd, MSG_215);
        } else if (strcmp(op, "FEAT") == 0) {
            send_ans(sd, MSG_211);
        } else if (strcmp(op, "PASV") == 0)  { // server listens for a connection on a certain port
            pasv(sd, &data_chnl);
        } else {
            send_ans(sd, MSG_502);
        }
    }
}

int send_pasv_ans(int cmd_chnl_sd, int data_chnl_sd) 
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int a, b, c, d, port, gsn;
    char ipv4_str[INET_ADDRSTRLEN];

    gsn = getsockname(data_chnl_sd, (struct sockaddr*) &addr, &len);
    if(gsn == -1)
        return 0;

    inet_ntop(AF_INET, &addr, ipv4_str, INET_ADDRSTRLEN);
    port = htons(addr.sin_port);
    sscanf(ipv4_str,"%d.%d.%d.%d", &a, &b, &c, &d);
    send_ans(cmd_chnl_sd, MSG_227, a, b, c, d, port / 256, port % 256);
    return 1;
}

void pasv(int cmd_chnl, FILE **data_chnl)
{
    char port[] = "3490";
    int data_sd;
    struct sockaddr_storage addr_stor;
    socklen_t len = sizeof(addr_stor);

    if(*data_chnl) return;

    data_sd = tcp_listen(port, 1);

    if(!data_sd) {
        send_ans(cmd_chnl, MSG_502);
        fprintf(stderr, "Error while opening sockets\n");
        return;
    }

    send_pasv_ans(cmd_chnl, data_sd); // Error checking 

    data_sd = accept(data_sd, (struct sockaddr *)&addr_stor,&len);

    *data_chnl = fdopen(data_sd, "r+");
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
