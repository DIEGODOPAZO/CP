#include <stdlib.h>

// circular array
typedef struct _queue {
    int size;
    int used;
    int first;
    void **data;
} _queue;

#include "queue.h"

queue q_create(int size) {
    queue q = malloc(sizeof(_queue));

    q->size  = size;
    q->used  = 0;
    q->first = 0;
    q->data  = malloc(size * sizeof(void *));

    return q;
}

int q_elements(queue q) {
    return q->used;
}

int q_insert(queue q, void *elem) {
    if(q->size == q->used) return -1;

    q->data[(q->first + q->used) % q->size] = elem;
    q->used++;

    return 0;
}

void *q_remove(queue q) {
    void *res;
    if(q->used == 0) return NULL;

    res = q->data[q->first];

    q->first = (q->first + 1) % q->size;
    q->used--;

    return res;
}

void q_destroy(queue q) {
    free(q->data);
    free(q);
}
