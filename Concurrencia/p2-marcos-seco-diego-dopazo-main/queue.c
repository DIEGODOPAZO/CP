#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>

// circular array
typedef struct _queue {
    int size;
    int used;
    int first;
    pthread_mutex_t *queue_mutex;
    pthread_cond_t *empty_cond;
    pthread_cond_t *full_cond;
    void **data;
    bool finish;
} _queue;

#include "queue.h"

queue q_create(int size) {

    queue q = malloc(sizeof(_queue));

    //initialisig the mutex and the consitions
    q->queue_mutex = malloc(sizeof(pthread_mutex_t));
    q->empty_cond = malloc(sizeof(pthread_cond_t));
    q->full_cond = malloc(sizeof(pthread_cond_t));
    pthread_mutex_init(q->queue_mutex, NULL);
    pthread_cond_init(q->empty_cond, NULL);
    pthread_cond_init(q->full_cond, NULL);

    q->size  = size;
    q->used  = 0;
    q->first = 0;
    q->finish = false;
    q->data  = malloc(size * sizeof(void *));

    return q;
}

void noMoreElements(queue q){
    pthread_mutex_lock(q->queue_mutex);
    q->finish = true;
    pthread_cond_broadcast(q->empty_cond);
    pthread_mutex_unlock(q->queue_mutex);
}

int q_elements(queue q) {
    return q->used;
}

int q_insert(queue q, void *elem) {

    pthread_mutex_lock(q->queue_mutex);

    while(q->size == q->used){ //waits untill the queue is not full
        pthread_cond_wait(q->full_cond, q->queue_mutex);
    }

    q->data[(q->first + q->used) % q->size] = elem;
    q->used++;

    if(q->used == 1){
        pthread_cond_broadcast(q->empty_cond);
    }

    pthread_mutex_unlock(q->queue_mutex);




    return 0;
}

void *q_remove(queue q) {
    void *res;


    pthread_mutex_lock(q->queue_mutex);

    while(q->used == 0){ //waits untill there are elements to remove

        if(q->finish){
            pthread_mutex_unlock(q->queue_mutex);
            return NULL;
        }
        pthread_cond_wait(q->empty_cond, q->queue_mutex);

    }

    res = q->data[q->first];

    q->first = (q->first + 1) % q->size;
    q->used--;

    if(q->used == q->size - 1){
        pthread_cond_broadcast(q->full_cond);
    }


    pthread_mutex_unlock(q->queue_mutex);




    return res;
}

void q_destroy(queue q) {
    //destroying the mutex and the conditions
    pthread_mutex_destroy(q->queue_mutex);
    pthread_cond_destroy(q->empty_cond);
    pthread_cond_destroy(q->full_cond);


    free(q->queue_mutex);
    free(q->full_cond);
    free(q->empty_cond);
    free(q->data);
    free(q);
}
