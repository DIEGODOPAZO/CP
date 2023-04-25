#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "options.h"

#define DELAY_SCALE 1000


struct array {
    int size;
    int *arr;

};

/* The thread_data structure also stores the counter and the extra mutex in this part */

struct thread_data {
    int id;
    int iterations;
    int delay;
    struct array *arr;
    pthread_mutex_t *mutexArray;
    int *counter;
    pthread_mutex_t *mutex;

};

void apply_delay(int delay) {
    for (int i = 0; i < delay * DELAY_SCALE; i++); // waste time
}

int increment_decrement(int id, int iterations, int delay, struct array *arr, pthread_mutex_t *mutex_arr, pthread_mutex_t *mutex, int *counter) {
    int pos1, pos2, val1, val2;

    /* Comparing this exercise to the previous one we added this lock before the while in each of the functions to
     * avoid interferences */

    pthread_mutex_lock(mutex);
    while (iterations != *counter) {
        (*counter)++;

        pthread_mutex_unlock(mutex);

        pos1 = rand() % arr->size;

        do {
            pos2 = rand() % arr->size;
        } while (pos1 == pos2);

        if (pos1 < pos2) {
            pthread_mutex_lock(&mutex_arr[pos1]);
            pthread_mutex_lock(&mutex_arr[pos2]);
        } else {
            pthread_mutex_lock(&mutex_arr[pos2]);
            pthread_mutex_lock(&mutex_arr[pos1]);
        }

        printf("%d decreasing position %d, increasing position %d\n", id, pos1, pos2);

        val1 = arr->arr[pos1];
        val2 = arr->arr[pos2];
        apply_delay(delay);

        val1--;
        val2++;
        apply_delay(delay);

        arr->arr[pos1] = val1;
        arr->arr[pos2] = val2;


        apply_delay(delay);

        pthread_mutex_unlock(&mutex_arr[pos1]);
        pthread_mutex_unlock(&mutex_arr[pos2]);

        pthread_mutex_lock(mutex);
    }
    pthread_mutex_unlock(mutex);
    return 0;
}

int increment(int id, int iterations, int delay, struct array *arr, pthread_mutex_t *mutex_arr, pthread_mutex_t *mutex, int *counter) {
    int pos, val;

    pthread_mutex_lock(mutex);
    while (iterations != *counter) {
        (*counter)++;

        pthread_mutex_unlock(mutex);

        pos = rand() % arr->size;

        pthread_mutex_lock(&mutex_arr[pos]);
        printf("%d increasing position %d\n", id, pos);

        val = arr->arr[pos];
        apply_delay(delay);

        val++;
        apply_delay(delay);


        arr->arr[pos] = val;
        apply_delay(delay);
        pthread_mutex_unlock(&mutex_arr[pos]);

        pthread_mutex_lock(mutex);
    }
    pthread_mutex_unlock(mutex);
    return 0;
}

void *routine(void *thread_data) {
    struct thread_data *data = (struct thread_data *) thread_data;
    increment(data->id, data->iterations, data->delay, data->arr, data->mutexArray, data->mutex, data->counter);
    pthread_exit(NULL);
}

void *routine2(void *thread_data) {
    struct thread_data *data = (struct thread_data *) thread_data;
    increment_decrement(data->id, data->iterations, data->delay, data->arr, data->mutexArray, data->mutex,data->counter);
    pthread_exit(NULL);
}


void print_array(struct array arr) {
    int total = 0;

    for (int i = 0; i < arr.size; i++) {
        total += arr.arr[i];
        printf("%d ", arr.arr[i]);
    }

    printf("\nTotal: %d\n", total);
}


int main(int argc, char **argv) {
    struct options opt;
    struct array arr;
    struct thread_data *thread_data_array;
    struct thread_data *new_thread_array;

    /* Shared counters are initialized here */

    int inc_counter = 0, inc_dec_counter = 0;

    /*These mutexes make sure that no thread enters the critical section of each function when they are locked */

    pthread_mutex_t mutex_increment, mutex_increment_decrement;

    srand(time(NULL));

    // Default values for the options
    opt.num_threads = 5;
    opt.size = 10;
    opt.iterations = 100;
    opt.delay = 1000;

    read_options(argc, argv, &opt);

    arr.size = opt.size;
    arr.arr = malloc(arr.size * sizeof(int));

    thread_data_array = (struct thread_data *) malloc(opt.num_threads * sizeof(struct thread_data));
    new_thread_array = (struct thread_data *) malloc(opt.num_threads * sizeof(struct thread_data));

    pthread_t th[opt.num_threads];
    pthread_t th2[opt.num_threads];

    pthread_mutex_t mutex_arr[arr.size];


    memset(arr.arr, 0, arr.size * sizeof(int));

    pthread_mutex_init(&mutex_increment, NULL);
    pthread_mutex_init(&mutex_increment_decrement, NULL);

    for (int i = 0; i < arr.size; i++) {
        pthread_mutex_init(&mutex_arr[i], NULL);
    }

    for (int i = 0; i < opt.num_threads; i++) {
        thread_data_array[i].id = i;
        thread_data_array[i].iterations = opt.iterations;
        thread_data_array[i].delay = opt.delay;
        thread_data_array[i].arr = &arr;
        thread_data_array[i].mutexArray = mutex_arr;
        thread_data_array[i].mutex = &mutex_increment;
        thread_data_array[i].counter = &inc_counter;

        new_thread_array[i].id = opt.num_threads + i;
        new_thread_array[i].iterations = opt.iterations;
        new_thread_array[i].delay = opt.delay;
        new_thread_array[i].arr = &arr;
        new_thread_array[i].mutexArray = mutex_arr;
        new_thread_array[i].mutex = &mutex_increment_decrement;
        new_thread_array[i].counter = &inc_dec_counter;

        if (pthread_create(&th2[i], NULL, routine2, &new_thread_array[i]) != 0) {
            perror("Failed to create thread\n");
        }

        if (pthread_create(&th[i], NULL, routine, &thread_data_array[i]) != 0) {
            perror("Failed to create thread\n");
        }

    }


    for (int i = 0; i < opt.num_threads; i++) {
        if (pthread_join(th[i], NULL) != 0) {
            perror("Failed to join thread\n");
        }
        if (pthread_join(th2[i], NULL) != 0) {
            perror("Failed to join thread\n");
        }
    }

    print_array(arr);

    for (int i = 0; i < arr.size; i++) {
        pthread_mutex_destroy(&mutex_arr[i]);
    }
    pthread_mutex_destroy(&mutex_increment);
    pthread_mutex_destroy(&mutex_increment_decrement);

    free(thread_data_array);
    free(new_thread_array);
    free(arr.arr);


    return 0;
}


