#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include "queue.h"

struct queue {
    int SIZE;
    void **buf;
    int in;
    int out;
    sem_t *full_spaces;
    sem_t *empty_spaces;
    sem_t *lock;
};

/** @brief Dynamically allocates and initializes a new queue with a
 *         maximum size, size
 *
 *  @param size the maximum size of the queue
 *
 *  @return a pointer to a new queue_t
 */
queue_t *queue_new(int size) {
    struct queue *q = calloc(1, sizeof(struct queue));
    q->SIZE = size;
    q->empty_spaces = calloc(1, sizeof(sem_t));
    q->full_spaces = calloc(1, sizeof(sem_t));
    q->lock = calloc(1, sizeof(sem_t));

    // Init to number of empty_spaces to the max size so calls to
    // queue_append() don't block until the queue is full
    int ret = sem_init(q->empty_spaces, 0, q->SIZE);
    assert(!ret);
    // Init to 0 because queue is empty at first and so calls to
    // queue_pop() before queue_append() block until there exists
    // an element in the queue
    ret = sem_init(q->full_spaces, 0, 0);
    assert(!ret);
    // Init to 1 so any calls to push or pop don't block yet
    ret = sem_init(q->lock, 0, 1);
    assert(!ret);
    q->buf = calloc(q->SIZE, sizeof(void *));

    return q;
}

/** @brief Delete your queue and free all of its memory.
 *
 *  @param q the queue to be deleted.  Note, you should assign the
 *  passed in pointer to NULL when returning (i.e., you should set
 *  *q = NULL after deallocation).
 *
 */
void queue_delete(queue_t **q) {
    // Invalid queue
    if (!q || !*q)
        return;
    // Free & destroy everything
    free((*q)->buf);
    int ret = sem_destroy((*q)->full_spaces);
    assert(!ret);
    ret = sem_destroy((*q)->empty_spaces);
    assert(!ret);
    ret = sem_destroy((*q)->lock);
    assert(!ret);

    free((*q)->full_spaces);
    free((*q)->empty_spaces);
    free((*q)->lock);

    // Free & set to NULL
    free(*q);
    *q = NULL;
}

/** @brief push an element onto a queue
 *
 *  @param q the queue to push an element into.
 *
 *  @param elem th element to add to the queue
 *
 *  @return A bool indicating success or failure.  Note, the function
 *          should succeed unless the q parameter is NULL.
 */
bool queue_push(queue_t *q, void *elem) {
    if (!q)
        return false;
    // Wait until the queue is NOT full
    sem_wait(q->empty_spaces);
    // Wait to acquire the push lock
    sem_wait(q->lock);
    q->buf[q->in] = elem;
    q->in = (q->in + 1) % q->SIZE;
    //fprintf(stderr, "Done pushing %p\n", elem);
    sem_post(q->lock);
    sem_post(q->full_spaces);
    return true;
}

/** @brief pop an element from a queue.
 *
 *  @param q the queue to pop an element from.
 *
 *  @param elem a place to assign the poped element.
 *
 *  @return A bool indicating success or failure.  Note, the function
 *          should succeed unless the q parameter is NULL.
 */
bool queue_pop(queue_t *q, void **elem) {
    if (!q)
        return false;
    // Wait until the queue is NOT empty
    sem_wait(q->full_spaces);
    // Wait until it can acquire the pop lock
    sem_wait(q->lock);
    *elem = q->buf[q->out];
    q->out = (q->out + 1) % q->SIZE;
    //fprintf(stderr, "Done popping %p\n", *elem);
    sem_post(q->lock);
    sem_post(q->empty_spaces);
    return true;
}
