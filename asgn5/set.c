#include <stdlib.h>
#include <string.h>
#include <limits.h>

/*@brief Creates a new set struct
 */
cache_set_t *set_create() {
	cache_set_t *set = calloc(1, sizeof(cache_set_t));
	return set;
}

/*@brief Deletes the linked list recursively
 */
void delete_nodes(cache_node *curr) {	
	if (!curr)
		return;
	cache_node *next = curr->next;
	free(curr->elem);
	free(curr);
	delete_nodes(next);
}

/*@brief Deletes the set struct
 */
void set_delete(cache_set_t **set) {
	if (!*set || !(*set)->head)
		return;
	delete_nodes((*set)->head);
	free(*set);
	*set = NULL;
}


/*@brief Checks to see if the set contains an elem
 */
bool set_contains(cache_set_t *set, char *elem) {
	if (!set || !set->head)
			return false;

	cache_node *curr = set->head;
	while (curr) {
		if (!strcmp(curr->elem, elem))
			return true;
		curr = curr->next;
	}
	return false;
}

