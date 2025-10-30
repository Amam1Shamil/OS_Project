#include "users.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <pthread.h>
#include <dirent.h>

#define USERS_FILE "users.txt"
#define USERS_DIR "users"
#define MAX_LINE_LEN 256

static pthread_mutex_t user_file_lock = PTHREAD_MUTEX_INITIALIZER;

void setup_user_system() {
    mkdir(USERS_DIR, 0755);
    pthread_mutex_lock(&user_file_lock);
    FILE *file = fopen(USERS_FILE, "a");
    if (file) {
        fclose(file);
    }
    pthread_mutex_unlock(&user_file_lock);
}

int create_new_user(const char* username, const char* password) {
    char line[MAX_LINE_LEN];
    int success = 0;

    pthread_mutex_lock(&user_file_lock);

    FILE* user_file = fopen(USERS_FILE, "r");
    if (user_file) {
        int user_exists = 0;
        while (fgets(line, sizeof(line), user_file)) {
            char existing_user[128];
            sscanf(line, "%127[^:]", existing_user);
            if (strcmp(existing_user, username) == 0) {
                user_exists = 1;
                break;
            }
        }
        fclose(user_file);
        if (user_exists) {
            pthread_mutex_unlock(&user_file_lock);
            return 0;
        }
    }

    user_file = fopen(USERS_FILE, "a");
    if (user_file) {
        fprintf(user_file, "%s:%s\n", username, password);
        fclose(user_file);
        success = 1;
    }
    pthread_mutex_unlock(&user_file_lock);

    if (success) {
        char user_dir_path[256];
        snprintf(user_dir_path, sizeof(user_dir_path), "%s/%s", USERS_DIR, username);
        mkdir(user_dir_path, 0755);
    }
    return success;
}

int check_user_login(const char* username, const char* password) {
    char line[MAX_LINE_LEN];
    char expected_line[MAX_LINE_LEN];
    snprintf(expected_line, sizeof(expected_line), "%s:%s\n", username, password);
    int credentials_match = 0;

    pthread_mutex_lock(&user_file_lock);

    FILE* user_file = fopen(USERS_FILE, "r");
    if (!user_file) {
        pthread_mutex_unlock(&user_file_lock);
        return 0;
    }

    while (fgets(line, sizeof(line), user_file)) {
        if (strcmp(line, expected_line) == 0) {
            credentials_match = 1;
            break;
        }
    }

    fclose(user_file);
    pthread_mutex_unlock(&user_file_lock);
    return credentials_match;
}

long get_user_storage_usage(const char* username) {
    char user_dir_path[256];
    snprintf(user_dir_path, sizeof(user_dir_path), "%s/%s", USERS_DIR, username);

    DIR *dirp = opendir(user_dir_path);
    if (!dirp) {
        return 0;
    }

    long total_size = 0;
    struct dirent *entry;
    struct stat file_stat;

    while ((entry = readdir(dirp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", user_dir_path, entry->d_name);
        if (stat(full_path, &file_stat) == 0) {
            total_size += file_stat.st_size;
        }
    }

    closedir(dirp);
    return total_size;
}