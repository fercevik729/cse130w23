typedef struct cache_node {
	char *elem;
	struct cache_node *next;
} cache_node;

typedef struct cache_set_t {
	cache_node *head;
}
