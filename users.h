#ifndef USERS_H
#define USERS_H

void setup_user_system();
int create_new_user(const char* username, const char* password);
int check_user_login(const char* username, const char* password);
long get_user_storage_usage(const char* username);

#endif