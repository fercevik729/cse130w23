#pragma once

#include "connection.h"
#include "queue.h"
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>

void handle_connection(int);

void handle_get(conn_t *);
void handle_put(conn_t *);
void handle_unsupported(conn_t *);

// THREAD POOL CODE
/** @struct thread_pool_t
*/
typedef struct thread_pool thread_pool_t;

/** @brief Dynamically allocates and initializes a new thread pool with a certain
 *         number of threads, an associated task queue, & a max buffer size
 *
 *  @param num_threads number of worker threads in the thread pool
 *
 *  @param q pointer to the shared thread-safe task queue 
 *
 *
 *  @return a pointer to a new queue_t
 */
thread_pool_t *thread_pool_new(size_t num_threads);
