#include <stdio.h>
#include <stdlib.h>
#include "hash_table.h"

hash_table_t *hash_init(int size) {
	hash_table_t *ht = malloc(sizeof(hash_table_t));
	ht->size = size * 2;
	ht->entries = calloc(ht->size, sizeof(hash_entry_t));
	return ht;
}

void hash_clear(hash_table_t *ht) {
	for (int i = 0; i < ht->size; i++) ht->entries[i].valid = 0;
}

void hash_destroy(hash_table_t *ht) {
	free(ht->entries);
	free(ht);
}

int hash_lookup(hash_table_t *ht, unsigned long page) {
	int hash_index = hash_func(ht, page);
	for (int i = 0; i < ht->size; i++) {
		int probe = (hash_index + i) % ht->size;
		if (!ht->entries[probe].valid) return -1; /* Not found */
		if (ht->entries[probe].page == page) 
			return ht->entries[probe].frame_index;
	}
	return -1; /* Table completely full but no match */
}

void hash_insert(hash_table_t *ht, unsigned long page, int frame_index) {
	int hash_index = hash_func(ht, page);
	for (int i = 0; i < ht->size; i++) {
		int probe = (hash_index + i) % ht->size;
		if (!ht->entries[probe].valid) {
			ht->entries[probe].valid = 1;
			ht->entries[probe].page = page;
			ht->entries[probe].frame_index = frame_index;
			return;
		}
	}
	fprintf(stderr, "[MM] Hash insert failed: table full!\n");
}

void hash_remove(hash_table_t *ht, unsigned long page) {
	int hash_index = hash_func(ht, page);
	for (int i = 0; i < ht->size; i++) {
		int probe = (hash_index + i) % ht->size;
		if (!ht->entries[probe].valid) return; /* Not found */
		if (ht->entries[probe].page == page) {
			ht->entries[probe].valid = 0;
			return;
		}
	}
}