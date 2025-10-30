#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <signal.h>
#include <sys/select.h>

#include "queue.h"
#include "users.h"
#include "concurrency.h"

#define PORT 8080
#define CLIENT_HANDLER_THREADS 4
#define WORKER_THREADS 8
#define RECV_BUF_SIZE 4096
#define USER_QUOTA (1024 * 1024)

SafeQueue *connection_queue;
SafeQueue *task_queue;

volatile sig_atomic_t server_running = 1;

typedef struct {
    int sock_fd;
    char username[128];
    int is_authed;
} client_t;

typedef struct {
    client_t *client_info;
    char command_line[1024];
    char *file_data;
    long file_size;
    char result_message[1024];
    int task_complete;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} worker_task_t;

void signal_handler(int signum) {
    if (signum == SIGINT) {
        printf("\nCaught signal %d, initiating shutdown...\n", signum);
        server_running = 0;
    }
}

void rot13_transform(char *str, size_t len) { for (size_t i = 0; i < len; ++i) { char c = str[i]; if (c >= 'a' && c <= 'z') str[i] = (((c - 'a') + 13) % 26) + 'a'; else if (c >= 'A' && c <= 'Z') str[i] = (((c - 'A') + 13) % 26) + 'A'; } }
void strip_crlf(char *str) { char *end = str + strlen(str) - 1; while (end >= str && (*end == '\n' || *end == '\r')) { *end = '\0'; end--; } }
int send_all(int sockfd, const char *buf, size_t len) { size_t total_sent = 0; while (total_sent < len) { ssize_t sent = send(sockfd, buf + total_sent, len - total_sent, 0); if (sent == -1) return -1; total_sent += sent; } return 0; }
void *worker_thread_func(void *arg);
void *client_handler_thread_func(void *arg);

int main() {
    signal(SIGINT, signal_handler);

    printf("Server starting...\n");
    setup_user_system();
    connection_queue = create_queue();
    task_queue = create_queue();

    pthread_t client_threads[CLIENT_HANDLER_THREADS];
    for (int i = 0; i < CLIENT_HANDLER_THREADS; i++) {
        pthread_create(&client_threads[i], NULL, client_handler_thread_func, NULL);
    }

    pthread_t workers[WORKER_THREADS];
    for (int i = 0; i < WORKER_THREADS; i++) {
        pthread_create(&workers[i], NULL, worker_thread_func, NULL);
    }

    int server_fd;
    struct sockaddr_in address;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 10);

    printf("Listening for connections on port %d\n", PORT);

    while (server_running) {
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(server_fd, &fds);

        int ret = select(server_fd + 1, &fds, NULL, NULL, &tv);
        if (ret > 0) {
            int client_fd = accept(server_fd, NULL, NULL);
            if (client_fd < 0) continue;
            int *conn_fd_ptr = (int*)malloc(sizeof(int));
            *conn_fd_ptr = client_fd;
            enqueue(connection_queue, conn_fd_ptr, 0);
        }
    }

    printf("Shutting down server...\n");

    for (int i = 0; i < CLIENT_HANDLER_THREADS; i++) {
        enqueue(connection_queue, NULL, 0);
    }
    for (int i = 0; i < CLIENT_HANDLER_THREADS; i++) {
        pthread_join(client_threads[i], NULL);
    }
    printf("Client handler threads joined.\n");

    for (int i = 0; i < WORKER_THREADS; i++) {
        enqueue(task_queue, NULL, 0);
    }
    for (int i = 0; i < WORKER_THREADS; i++) {
        pthread_join(workers[i], NULL);
    }
    printf("Worker threads joined.\n");

    close(server_fd);
    destroy_queue(connection_queue);
    destroy_queue(task_queue);
    printf("Server shut down cleanly.\n");
    return 0;
}

void *client_handler_thread_func(void *arg) {
    while (1) {
        int *client_fd_ptr = (int*)dequeue(connection_queue);
        if (client_fd_ptr == NULL) {
            break;
        }
        int client_fd = *client_fd_ptr;
        free(client_fd_ptr);

        client_t client = { .sock_fd = client_fd, .is_authed = 0 };
        char recv_buf[RECV_BUF_SIZE];

        send_all(client_fd, "Welcome! Awaiting SIGNUP or LOGIN.\n", 36);

        while (server_running && !client.is_authed) {
            int len = recv(client_fd, recv_buf, sizeof(recv_buf) - 1, 0);
            if (len <= 0) break;
            recv_buf[len] = '\0';
            strip_crlf(recv_buf);

            char *command = strtok(recv_buf, " ");
            char *username = strtok(NULL, " ");
            char *password = strtok(NULL, " ");

            if (!command) continue;

            if (strcmp(command, "SIGNUP") == 0 && username && password) {
                if (create_new_user(username, password)) send_all(client_fd, "OK: User created. Please LOGIN.\n", 32);
                else send_all(client_fd, "ERR: Username taken.\n", 22);
            } else if (strcmp(command, "LOGIN") == 0 && username && password) {
                if (check_user_login(username, password)) {
                    client.is_authed = 1;
                    strncpy(client.username, username, sizeof(client.username) - 1);
                    send_all(client_fd, "OK: Logged in.\n", 17);
                } else send_all(client_fd, "ERR: Bad credentials.\n", 22);
            } else send_all(client_fd, "ERR: Invalid command. Use SIGNUP or LOGIN.\n", 44);
        }

        while (server_running && client.is_authed) {
            int len = recv(client_fd, recv_buf, sizeof(recv_buf) - 1, 0);
            if (len <= 0) break;
            recv_buf[len] = '\0';
            strip_crlf(recv_buf);

            if (strlen(recv_buf) == 0) continue;

            worker_task_t *task = malloc(sizeof(worker_task_t));
            task->client_info = malloc(sizeof(client_t));
            memcpy(task->client_info, &client, sizeof(client_t));
            strncpy(task->command_line, recv_buf, sizeof(task->command_line) - 1);
            
            pthread_mutex_init(&task->lock, NULL);
            pthread_cond_init(&task->cond, NULL);
            task->task_complete = 0;
            task->file_data = NULL;
            task->file_size = 0;

            int priority = 1;
            char *command_copy = strdup(recv_buf);
            char *command = strtok(command_copy, " ");
            char *filename = strtok(NULL, " ");
            char *size_str = strtok(NULL, " ");
            
            if (command && strcmp(command, "UPLOAD") == 0 && filename && size_str) {
                priority = 3;
                task->file_size = atol(size_str);
                if (task->file_size > 0 && task->file_size <= USER_QUOTA) {
                    task->file_data = malloc(task->file_size);
                    long total_read = 0;
                    while (total_read < task->file_size) {
                        int n = recv(client_fd, task->file_data + total_read, task->file_size - total_read, 0);
                        if (n <= 0) break;
                        total_read += n;
                    }
                    if (total_read != task->file_size) {
                        free(task->file_data); free(task->client_info); free(task);
                        send_all(client_fd, "ERR: Incomplete upload data.\n", 29);
                        continue;
                    }
                } else {
                    free(task->client_info); free(task);
                    send_all(client_fd, "ERR: File size is zero or too large.\n", 37);
                    continue;
                }
            } else if (command && strcmp(command, "DOWNLOAD") == 0) {
                priority = 2;
            }
            free(command_copy);

            enqueue(task_queue, task, priority);
            
            pthread_mutex_lock(&task->lock);
            while (!task->task_complete) {
                pthread_cond_wait(&task->cond, &task->lock);
            }
            pthread_mutex_unlock(&task->lock);

            if (strlen(task->result_message) > 0) {
                send_all(client_fd, task->result_message, strlen(task->result_message));
            }

            if (task->file_data && task->file_size > 0 && strncmp(task->command_line, "DOWNLOAD", 8) == 0) {
                send_all(client_fd, task->file_data, task->file_size);
            }
            
            pthread_mutex_destroy(&task->lock);
            pthread_cond_destroy(&task->cond);
            if (task->file_data) free(task->file_data);
            free(task->client_info);
            free(task);
        }
        
        printf("Client on fd %d disconnected.\n", client_fd);
        close(client_fd);
    }
    return NULL;
}

void *worker_thread_func(void *arg) {
    while (1) {
        worker_task_t *task = (worker_task_t*)dequeue(task_queue);
        if (task == NULL) {
            break;
        }
        client_t *client = task->client_info;
        
        lock_user(client->username);

        char command_line_copy[1024];
        strncpy(command_line_copy, task->command_line, sizeof(command_line_copy));
        char *command = strtok(command_line_copy, " ");
        char *filename = strtok(NULL, " ");

        char user_path[512];
        if (filename) snprintf(user_path, sizeof(user_path), "users/%s/%s", client->username, filename);

        task->result_message[0] = '\0';

        if (command && strcmp(command, "LIST") == 0) {
            char user_dir[256];
            snprintf(user_dir, sizeof(user_dir), "users/%s", client->username);
            DIR *dirp = opendir(user_dir);
            if (dirp) {
                struct dirent *entry;
                char list_buf[RECV_BUF_SIZE] = "Files:\n------\n";
                while ((entry = readdir(dirp)) != NULL) {
                    if (entry->d_name[0] != '.') {
                        strncat(list_buf, entry->d_name, sizeof(list_buf) - strlen(list_buf) - 1);
                        strncat(list_buf, "\n", sizeof(list_buf) - strlen(list_buf) - 1);
                    }
                }
                closedir(dirp);
                strcpy(task->result_message, list_buf);
            }
        } else if (command && strcmp(command, "DELETE") == 0 && filename) {
            if (remove(user_path) == 0) strcpy(task->result_message, "OK: File deleted.\n");
            else strcpy(task->result_message, "ERR: Delete failed.\n");
        } else if (command && strcmp(command, "DOWNLOAD") == 0 && filename) {
            FILE *file = fopen(user_path, "rb");
            if (file) {
                fseek(file, 0, SEEK_END); long file_size = ftell(file); fseek(file, 0, SEEK_SET);
                task->file_data = malloc(file_size); task->file_size = file_size;
                fread(task->file_data, 1, file_size, file); fclose(file);
                rot13_transform(task->file_data, file_size);
                snprintf(task->result_message, sizeof(task->result_message), "%ld\n", file_size);
            } else strcpy(task->result_message, "0\n");
        } else if (command && strcmp(command, "UPLOAD") == 0 && filename) {
            long current_usage = get_user_storage_usage(client->username);
            if (current_usage + task->file_size > USER_QUOTA) {
                strcpy(task->result_message, "ERR: Quota exceeded. File not uploaded.\n");
            } else {
                rot13_transform(task->file_data, task->file_size); 
                FILE *outfile = fopen(user_path, "wb");
                if (outfile) {
                    fwrite(task->file_data, 1, task->file_size, outfile); fclose(outfile);
                    strcpy(task->result_message, "OK: Uploaded.\n");
                } else strcpy(task->result_message, "ERR: Server failed to save.\n");
            }
        } else {
            strcpy(task->result_message, "ERR: Unknown command.\n");
        }

        unlock_user(client->username);

        pthread_mutex_lock(&task->lock);
        task->task_complete = 1;
        pthread_cond_signal(&task->cond);
        pthread_mutex_unlock(&task->lock);
    }
    return NULL;
}