#include "cache.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

struct cache_elem {
    char *elem;

    // Metadata for cache elements
    int created; // Used for FIFO
    int last_ref; // Used for LRU
    bool is_ref; // Used for CLK
};

/** @brief Replaces a cache element with a new element and updates references
 * 
*/
void replace_cache_elem(struct cache_elem *rep, char *new_elem, int ref) {
    // Free old str
    free(rep->elem);
    // Create copy of new str
    rep->elem = strndup(new_elem, LINE_MAX);
    rep->created = ref;
    rep->last_ref = ref;
    rep->is_ref = true;
}

struct cache {
    int capacity;
    int curr_ref;
    enum cache_policy policy;
    struct cache_elem **elems;

    int clock_hand;
    int compuls_m;
    int capacity_m;
    char **seen_elems;
    int seen_size;
};

/** @brief Constructs a new cache 
 * @param N - maximum number of elements in the cache
 * @param policy - cache eviction policy
 * @return a ptr to a cache_t struct
*/
cache_t *cache_new(int N, enum cache_policy policy) {
    // Bad capacity
    if (N <= 0)
        return NULL;
    struct cache *c = calloc(1, sizeof(struct cache));
    c->capacity = N;
    c->policy = policy;
    c->elems = calloc(N, sizeof(struct cache_elem *));
    c->seen_elems = calloc(N, sizeof(char *));
    c->seen_size = N;
    return c;
}

/** @brief Updates the set of visited elements
 *
*/
void set_add(cache_t *c, char *elem) {
    char **set = c->seen_elems;
    for (int i = 0; i < c->seen_size; i++) {
        // Add it to first empty slot
        if (!set[i]) {
            set[i] = strndup(elem, LINE_MAX);
            return;
        }
        // If it is already in the set return early
        if (!strcmp(set[i], elem))
            return;
    }
    // Reallocate more data if set is full
    c->seen_elems = realloc(c->seen_elems, c->seen_size + c->capacity);
    c->seen_size += c->capacity;
    // Recurse once
    set_add(c, elem);
}

/** @brief Checks to see if the set of visited elements contains
 * the target element
 */
bool set_contains(char **set, int set_size, char *elem) {
    for (int i = 0; i < set_size; i++) {
        if (!set[i])
            return false;
        if (!strcmp(set[i], elem))
            return true;
    }
    return false;
}

/** @brief Accesses the cache for a particular element
 * @param c - pointer to the cache
 * @param elem - requested element
*/
void cache_access(cache_t *c, char *elem) {
    for (int i = 0; i < c->capacity; i++) {
        if (!strcmp(c->elems[i]->elem, elem)) {
            c->elems[i]->last_ref = c->curr_ref;
            c->elems[i]->is_ref = true;
            c->curr_ref++;
            fprintf(stdout, "HIT\n");
            return;
        }
    }
    // If the elem was not in the cache add it
    fprintf(stdout, "MISS\n");
    if (set_contains(c->seen_elems, c->seen_size, elem)) {
        c->capacity_m++;
    } else {
        c->compuls_m++;
    }
    cache_add(c, elem);
    set_add(c, elem);
    c->curr_ref++;
}

/** @brief Adds an element to the cache and evicts another if needed
 *  @param c - a pointer to a cache_t struct
 *  @param elem - an element of the user's choice 
*/
void cache_add(cache_t *c, char *elem) {

    // Ensures that cache is filled up completely before any kind of replacement
    for (int i = 0; i < c->capacity; i++) {
        if (!c->elems[i]) {
            c->elems[i] = calloc(1, sizeof(struct cache_elem));
            c->elems[i]->elem = strndup(elem, LINE_MAX);
            c->elems[i]->created = c->curr_ref;
            c->elems[i]->last_ref = c->curr_ref;
            c->elems[i]->is_ref = true;
	    return;
        }
    }
    // Otherwise evict based on different policies (modularize) & replace
    cache_replace(c, elem);
}

void cache_replace(cache_t *c, char *new_elem) {
    enum cache_policy pol = c->policy;

    switch (pol) {
    case FIFO: fifo_replace(c, new_elem); break;
    case LRU: lru_replace(c, new_elem); break;
    case CLK: clk_replace(c, new_elem); break;
    }
}

// POLICY-SPECIFIC FUNCTIONS
void fifo_replace(cache_t *c, void *new_elem) {
    int min_created = INT_MAX;
    int evict_idx = -1;

    for (int i = 0; i < c->capacity; i++) {
        if (c->elems[i]->created < min_created) {
            min_created = c->elems[i]->created;
            evict_idx = i;
        }
    }
    // Replace elem at evict_idx with new elem
    replace_cache_elem(c->elems[evict_idx], new_elem, c->curr_ref);
}

void lru_replace(cache_t *c, void *new_elem) {
    int min_ref = INT_MAX;
    int evict_idx = -1;

    for (int i = 0; i < c->capacity; i++) {
        if (c->elems[i]->last_ref < min_ref) {
            min_ref = c->elems[i]->last_ref;
            evict_idx = i;
        }
    }
    // Replace elem at evict_idx with new elem
    replace_cache_elem(c->elems[evict_idx], new_elem, c->curr_ref);
}

void clk_replace(cache_t *c, void *new_elem) {
    while (1) {
        // Break out on first elem that has a '0' bit
        if (!c->elems[c->clock_hand]) {
            break;
        }
        c->elems[c->clock_hand] = false;
        // Move hand
        c->clock_hand = (c->clock_hand + 1) % c->capacity;
    }
    // Replace that element
    replace_cache_elem(c->elems[c->clock_hand], new_elem, c->curr_ref);
}

