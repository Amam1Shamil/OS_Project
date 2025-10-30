#ifndef CONCURRENCY_H
#define CONCURRENCY_H

#include <pthread.h>

#define NUM_USER_LOCKS 128 

void lock_user(const char* username);

void unlock_user(const char* username);

#endif