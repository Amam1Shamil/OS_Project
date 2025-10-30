#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>

typedef struct QueueNode {
    void *item;
    int priority;
    struct QueueNode *next;
} QueueNode;

typedef struct {
    QueueNode *head;
    QueueNode *tail;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} SafeQueue;

SafeQueue* create_queue();
void destroy_queue(SafeQueue *q);
void enqueue(SafeQueue *q, void *item, int priority);
void* dequeue(SafeQueue *q);

#endif