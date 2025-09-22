#ifndef HASH_TABLE_H
#define HASH_TABLE_H

typedef struct {
	unsigned long page;
	int frame_index;
	int valid;
} hash_entry_t;

typedef struct {
	hash_entry_t *entries;
	int size;
} hash_table_t;

/* Compute hash index using simple modulo of table size */
static inline int hash_func(hash_table_t *ht, unsigned long page) {
	return page % ht->size;
}

/**
 * hash_init - allocate and initialize hash table
 * @size: number of frames (used to determine table size)
 *
 * Allocates a hash table with roughly double the number of frames
 * to reduce collisions and keep load factor < 0.5.
 */
hash_table_t *hash_init(int size);

/**
 * hash_clear - mark all entries as invalid
 * @ht: hash table to clear
 *
 * Resets validity bits but does not free memory.
 */
void hash_clear(hash_table_t *ht);

/**
 * hash_destroy - free all memory associated with table
 * @ht: hash table to destroy
 */
void hash_destroy(hash_table_t *ht);

/**
 * hash_lookup - find frame index for a given page
 * @ht:   hash table
 * @page: page number to search for
 *
 * Uses linear probing starting from hash index until a match
 * or an empty slot is found. Returns frame index or -1 if not found.
 */
int hash_lookup(hash_table_t *ht, unsigned long page);

/**
 * hash_insert - insert page->frame mapping into hash table
 * @ht:          hash table
 * @page:        page number
 * @frame_index: frame index associated with page
 *
 * Uses open addressing with linear probing. Should never fail
 * if table was correctly sized (load factor < 1).
 */
void hash_insert(hash_table_t *ht, unsigned long page, int frame_index);

/**
 * hash_remove - invalidate page mapping from table
 * @ht:   hash table
 * @page: page number to remove
 *
 * Marks slot as invalid but does not shift other entries. This
 * may leave "holes" that increase probe lengths slightly.
 */
void hash_remove(hash_table_t *ht, unsigned long page);

#endif /* HASH_TABLE_H */