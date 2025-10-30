#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_PORT 8080
#define SERVER_IP "127.0.0.1"
#define BUF_SIZE 4096

void strip_crlf(char *str) {
    char *end = str + strlen(str) - 1;
    while (end >= str && (*end == '\n' || *end == '\r')) *end-- = '\0';
}

int main(int argc, char const *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "USAGE: %s <username> <password> <command> [args...]\n", argv[0]);
        fprintf(stderr, "Commands:\n");
        fprintf(stderr, "  SIGNUP\n");
        fprintf(stderr, "  LOGIN (prints nothing on success)\n");
        fprintf(stderr, "  LIST\n");
        fprintf(stderr, "  UPLOAD <local_filepath>\n");
        fprintf(stderr, "  DELETE <remote_filename>\n");
        return 1;
    }

    const char* username = argv[1];
    const char* password = argv[2];
    const char* command  = argv[3];

    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUF_SIZE] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    if(inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return -1;
    }

    read(sock, buffer, BUF_SIZE); 

    if (strcmp(command, "SIGNUP") == 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "SIGNUP %s %s\n", username, password);
        send(sock, msg, strlen(msg), 0);
        int len = read(sock, buffer, BUF_SIZE - 1);
        buffer[len] = '\0';
        printf("%s", buffer);
    } else {
        
        char login_msg[256];
        snprintf(login_msg, sizeof(login_msg), "LOGIN %s %s\n", username, password);
        send(sock, login_msg, strlen(login_msg), 0);
        read(sock, buffer, BUF_SIZE); 

        if (strcmp(command, "LIST") == 0) {
            send(sock, "LIST\n", 5, 0);
            int len = read(sock, buffer, BUF_SIZE - 1);
            buffer[len] = '\0';
            printf("%s", buffer);
        } else if (strcmp(command, "DELETE") == 0 && argc > 4) {
            char msg[256];
            snprintf(msg, sizeof(msg), "DELETE %s\n", argv[4]);
            send(sock, msg, strlen(msg), 0);
            int len = read(sock, buffer, BUF_SIZE - 1);
            buffer[len] = '\0';
            printf("%s", buffer);
        } else if (strcmp(command, "UPLOAD") == 0 && argc > 4) {
            FILE* fp = fopen(argv[4], "rb");
            if (!fp) {
                fprintf(stderr, "Cannot open local file: %s\n", argv[4]);
                close(sock);
                return 1;
            }
            fseek(fp, 0, SEEK_END);
            long file_size = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            char* file_buf = malloc(file_size);
            fread(file_buf, 1, file_size, fp);
            fclose(fp);

            char msg[512];
            snprintf(msg, sizeof(msg), "UPLOAD %s %ld\n", argv[4], file_size);
            send(sock, msg, strlen(msg), 0);
            send(sock, file_buf, file_size, 0);
            free(file_buf);

            int len = read(sock, buffer, BUF_SIZE - 1);
            buffer[len] = '\0';
            printf("%s", buffer);
        }
    }

    close(sock);
    return 0;
}