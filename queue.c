#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

SafeQueue* create_queue() {
    SafeQueue *q = malloc(sizeof(SafeQueue));
    if (!q) {
        return NULL;
    }
    q->head = NULL;
    q->tail = NULL;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
    return q;
}

void destroy_queue(SafeQueue *q) {
    if (!q) return;

    QueueNode* current = q->head;
    while (current != NULL) {
        QueueNode* temp = current;
        current = current->next;
        free(temp);
    }
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->cond);
    free(q);
}

void enqueue(SafeQueue *q, void *item, int priority) {
    QueueNode *new_node = (QueueNode*)malloc(sizeof(QueueNode));
    new_node->item = item;
    new_node->priority = priority;
    new_node->next = NULL;

    pthread_mutex_lock(&q->lock);

    if (q->head == NULL || priority > q->head->priority) {
        new_node->next = q->head;
        q->head = new_node;
    } else {
        QueueNode *current = q->head;
        while (current->next != NULL && current->next->priority >= priority) {
            current = current->next;
        }
        new_node->next = current->next;
        current->next = new_node;
    }

    if (new_node->next == NULL) {
        q->tail = new_node;
    }
    if (q->head->next == NULL) {
        q->tail = q->head;
    }


    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->lock);
}

void* dequeue(SafeQueue *q) {
    pthread_mutex_lock(&q->lock);

    while (q->head == NULL) {
        pthread_cond_wait(&q->cond, &q->lock);
    }

    QueueNode *temp_node = q->head;
    void *item = temp_node->item;
    q->head = q->head->next;

    if (q->head == NULL) {
        q->tail = NULL;
    }

    pthread_mutex_unlock(&q->lock);

    free(temp_node);
    return item;
}