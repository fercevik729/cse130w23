typedef struct cache cache_t;

enum cache_policy {
    FIFO,
    LRU,
    CLK,
};

// Main cache_t functions
cache_t *cache_new(int N, enum cache_policy policy);
void cache_access(cache_t *c, char *elem);
void cache_add(cache_t *c, char *elem);
void cache_replace(cache_t *c, char *new_elem);

// POLICY-specific functions
void fifo_replace(cache_t *c, void *new_elem);
void lru_replace(cache_t *c, void *new_elem);
void clk_replace(cache_t *c, void *new_elem);
