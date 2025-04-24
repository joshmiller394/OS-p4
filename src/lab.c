#include "lab.h"
#include <pthread.h>
#include <stdio.h>
#include <errno.h>

/*
 * Internal queue structure.  The opaque `struct queue` type forward‑declared
 * in lab.h is completed here.  All access is protected by `mutex` so the queue
 * is safe for multiple producer / multiple consumer scenarios.
 */
struct queue {
    void **buffer;           /* circular buffer holding `capacity` elements */
    int capacity;            /* maximum number of items            */

    int head;                /* index of next element to dequeue   */
    int tail;                /* index of next free slot to enqueue */
    int size;                /* current element count              */

    bool shutdown;           /* true once queue_shutdown() called  */

    pthread_mutex_t mutex;   /* protects every field in this struct */
    pthread_cond_t  not_empty; /* signalled when an item is added  */
    pthread_cond_t  not_full;  /* signalled when an item is removed*/
};

/***************************************************************************
 * Helper: allocate and initialise synchronisation primitives, returning
 * NULL if any step fails.                                                   *
 ***************************************************************************/
static queue_t alloc_queue(int capacity)
{
    if (capacity <= 0) {
        errno = EINVAL;
        return NULL;
    }

    queue_t q = malloc(sizeof(*q));
    if (!q)
        return NULL;

    q->buffer = malloc(sizeof(void *) * (size_t)capacity);
    if (!q->buffer) {
        free(q);
        return NULL;
    }

    q->capacity  = capacity;
    q->head = q->tail = q->size = 0;
    q->shutdown = false;

    if (pthread_mutex_init(&q->mutex, NULL) != 0 ||
        pthread_cond_init(&q->not_empty, NULL) != 0 ||
        pthread_cond_init(&q->not_full,  NULL) != 0) {
        /* Clean up on failure */
        pthread_mutex_destroy(&q->mutex);
        pthread_cond_destroy(&q->not_empty);
        pthread_cond_destroy(&q->not_full);
        free(q->buffer);
        free(q);
        return NULL;
    }
    return q;
}

/***************************** Public API **********************************/

queue_t queue_init(int capacity)
{
    return alloc_queue(capacity);
}

void queue_destroy(queue_t q)
{
    if (!q)
        return;

    /* Wake up any blocked threads before tearing down primitives */
    pthread_mutex_lock(&q->mutex);
    q->shutdown = true;
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->mutex);

    /* Now safe to destroy */
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
    free(q->buffer);
    free(q);
}

void enqueue(queue_t q, void *data)
{
    if (!q)
        return;

    pthread_mutex_lock(&q->mutex);

    /* Block while full and not shutdown */
    while (q->size == q->capacity && !q->shutdown)
        pthread_cond_wait(&q->not_full, &q->mutex);

    if (q->shutdown) {
        pthread_mutex_unlock(&q->mutex);
        return; /* Ignore enqueue after shutdown */
    }

    /* Insert item */
    q->buffer[q->tail] = data;
    q->tail = (q->tail + 1) % q->capacity;
    q->size++;

    /* Wake one waiting dequeuer */
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

void *dequeue(queue_t q)
{
    if (!q)
        return NULL;

    pthread_mutex_lock(&q->mutex);

    /* Block while empty and not shutdown */
    while (q->size == 0 && !q->shutdown)
        pthread_cond_wait(&q->not_empty, &q->mutex);

    /* If shutdown and empty → exit */
    if (q->size == 0) {
        pthread_mutex_unlock(&q->mutex);
        return NULL;
    }

    void *data = q->buffer[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->size--;

    /* Wake one waiting enqueuer */
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);

    return data;
}

void queue_shutdown(queue_t q)
{
    if (!q)
        return;

    pthread_mutex_lock(&q->mutex);
    q->shutdown = true;
    /* Wake everyone so they can notice the flag and exit */
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
}

bool is_empty(queue_t q)
{
    if (!q)
        return true;

    pthread_mutex_lock(&q->mutex);
    bool empty = (q->size == 0);
    pthread_mutex_unlock(&q->mutex);
    return empty;
}

bool is_shutdown(queue_t q)
{
    if (!q)
        return true;

    pthread_mutex_lock(&q->mutex);
    bool sd = q->shutdown;
    pthread_mutex_unlock(&q->mutex);
    return sd;
}
