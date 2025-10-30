#include "concurrency.h"
#include <string.h>

static pthread_mutex_t user_locks[NUM_USER_LOCKS] = { [0 ... (NUM_USER_LOCKS - 1)] = PTHREAD_MUTEX_INITIALIZER };

static unsigned int hash_username(const char* username) {
    unsigned int hash = 5381;
    int c;
    while ((c = *username++)) {
        hash = ((hash << 5) + hash) + c; 
    }
    return hash % NUM_USER_LOCKS;
}

void lock_user(const char* username) {
    unsigned int index = hash_username(username);
    pthread_mutex_lock(&user_locks[index]);
}

void unlock_user(const char* username) {
    unsigned int index = hash_username(username);
    pthread_mutex_unlock(&user_locks[index]);
}