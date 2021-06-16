/**
 * @file    hashtable.c
 * @brief   Contiene l'implementazione della hash table.
 * @author	Pierre-Henri Symoneaux
**/

#include "hashtable.h"

unsigned int ht_calc_hash(char* key) {
	unsigned int h = 5381;
	int c;
	while((c = *key++) != 0)
		h = ((h << 5) + h) + c;
	return h;
}


hashtable_t* ht_create(unsigned int capacity) {
	hashtable_t* hasht = malloc(sizeof(hashtable_t));
	if(!hasht)
		return NULL;
	if((hasht->table = malloc(capacity*sizeof(hash_elem_t*))) == NULL)
	{
		free(hasht->table);
		return NULL;
	}
	hasht->capacity = capacity;
	hasht->e_num = 0;
	unsigned int i;
	for(i = 0; i < capacity; i++)
		hasht->table[i] = NULL;
	return hasht;
}


void* ht_put(hashtable_t* hasht, char* key, void* data, size_t data_length, int lock, unsigned long updatedDate) {
	unsigned int h = ht_calc_hash(key) % hasht->capacity;
	hash_elem_t* e = hasht->table[h];

	while(e != NULL)
	{
		if(strcmp(e->key, key) == 0) { // se ho già trovato una chiave uguale
			e->data_length = data_length;
			strcpy(e->path, key);
			if(data_length != 0)
				memcpy(e->data, data, data_length);
			else
				e->data = NULL;
            e->lock = lock;
            e->updatedDate = updatedDate;
			pthread_mutex_init(e->mutex, NULL);
			return e->data;
		}
		e = e->next;
	}

	// Getting here means the key doesn't already exist

	if((e = malloc(sizeof(hash_elem_t)+strlen(key)+1)) == NULL)
		return HT_ERROR;
	strcpy(e->key, key);

	strcpy(e->path, key);
	if(data_length != 0)
		memcpy(e->data, data, data_length);
	else
		e->data = NULL;
	e->data_length = data_length;
	e->updatedDate = updatedDate;
	e->codaRichiedentiLock = NULL;
	e->lock = lock;
	e->mutex = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(e->mutex, NULL);

	e->rilascioLock = malloc(sizeof(pthread_cond_t));
	pthread_cond_init(e->rilascioLock, NULL);

	// Add the element at the beginning of the linked list
	e->next = hasht->table[h];
	hasht->table[h] = e;
	hasht->e_num ++;

	return NULL;
}


char* ht_getPath(hashtable_t* hasht, char* key) {
	unsigned int h = ht_calc_hash(key) % hasht->capacity;
	hash_elem_t* e = hasht->table[h];
	while(e != NULL)
	{
		if(!strcmp(e->key, key))
			return e->path;
		e = e->next;
	}
	return NULL;
}


void* ht_remove(hashtable_t* hasht, char* key) {
	unsigned int h = ht_calc_hash(key) % hasht->capacity;
	hash_elem_t* e = hasht->table[h];
	hash_elem_t* prev = NULL;
	while(e != NULL)
	{
		if(strcmp(e->key, key) == 0) // trovato
		{
			//void* ret = e->data;
			if(prev != NULL)
				prev->next = e->next;
			else
				hasht->table[h] = e->next;
			// free(e->path); // FIXME
			free(e->data);
			free(e->mutex);
			deleteList(e->codaRichiedentiLock);
			free(e->rilascioLock);
			hasht->e_num --;
			return e;
		}
		prev = e;
		e = e->next;
	}
	return NULL;
}


hash_elem_t* ht_getElement(hashtable_t* hasht, char* key) {
	if(key == NULL) {
		return NULL;
	}

	unsigned int h = ht_calc_hash(key) % hasht->capacity;
	hash_elem_t* e = hasht->table[h];
	while(e != NULL)
	{
		if(!strcmp(e->key, key))
			return e;
		e = e->next;
	}
	return NULL;
}


hash_elem_t* ht_iterate(hash_elem_it* iterator) {
	while(iterator->elem == NULL)
	{
		if(iterator->index < iterator->ht->capacity - 1)
		{
			iterator->index++;
			iterator->elem = iterator->ht->table[iterator->index];
		}
		else
			return NULL;
	}
	hash_elem_t* e = iterator->elem;
	if(e)
		iterator->elem = e->next;

	return e;
}


char* ht_iterate_keys(hash_elem_it* iterator) {
	hash_elem_t* e = ht_iterate(iterator);
	return (e == NULL ? NULL : e->key);
}


void* ht_iterate_values(hash_elem_it* iterator) {
	hash_elem_t* e = ht_iterate(iterator);
	return (e == NULL ? NULL : e->data);
}


void ht_clear(hashtable_t* hasht) {
	hash_elem_it it = HT_ITERATOR(hasht);
	char* k = ht_iterate_keys(&it);
	while(k != NULL) {
		free(ht_remove(hasht, k));
		k = ht_iterate_keys(&it);
	}
}


void ht_destroy(hashtable_t* hasht) {
	ht_clear(hasht); // Delete and free all.
	free(hasht->table);
	free(hasht);
}


hash_elem_t* ht_find_old_entry(hashtable_t* hasht) {
	hash_elem_it it = HT_ITERATOR(hasht);
	hash_elem_t* k = ht_iterate(&it);

	hash_elem_t* vecchio = NULL;
	

	while(k != NULL)
	{
		if(vecchio == NULL) {
			vecchio = malloc(sizeof(hash_elem_t));
			*vecchio = *k;
		} else {
			if(k->updatedDate < vecchio->updatedDate) {
				*vecchio = *k;
			}
		}
		k = ht_iterate(&it);
	}

	free(k);

	return vecchio;
}
