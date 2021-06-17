/**
 * @file    hashtable.h
 * @brief   Contiene l'header delle funzioni che lavorano sulla hash table.
 * @author  Leonardo Pantani & Pierre-Henri Symoneaux
**/

#ifndef HASH_TABLE_H_
#define HASH_TABLE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "list.h"

//Hashtable element structure
typedef struct hash_elem_t {
	struct hash_elem_t* next; // Next element in case of a collision
	void* data;	// Pointer to the stored element
	char key[]; 	// Key of the stored element
} hash_elem_t;

//Hashtabe structure
typedef struct {
	unsigned int capacity;	// Hashtable capacity (in terms of hashed keys)
	unsigned int e_num;	// Number of element currently stored in the hashtable
	hash_elem_t** table;	// The table containaing elements
} hashtable_t;

//Structure used for iterations
typedef struct {
	hashtable_t* ht; 	// The hashtable on which we iterate
	unsigned int index;	// Current index in the table
	hash_elem_t* elem; 	// Curent element in the list
} hash_elem_it;

// Inititalize hashtable iterator on hashtable 'ht'
#define HT_ITERATOR(ht) {ht, 0, ht->table[0]}

char err_ptr;
void* HT_ERROR = &err_ptr; // Data pointing to HT_ERROR are returned in case of error

/* 	Internal funcion to calculate hash for keys.
	It's based on the DJB algorithm from Daniel J. Bernstein.
	The key must be ended by '\0' character.*/
unsigned int ht_calc_hash(char* key);


/* 	Create a hashtable with capacity 'capacity'
	and return a pointer to it*/
hashtable_t* ht_create(unsigned int capacity);


/* 	Store data in the hashtable. If data with the same key are already stored,
	they are overwritten, and return by the function. Else it return NULL.
	Return HT_ERROR if there are memory alloc error*/
void* ht_put(hashtable_t* hasht, char* key, void* data);


/* Retrieve data from the hashtable */
void* ht_get(hashtable_t* hasht, char* key);


/* 	Remove data from the hashtable. Return the data removed from the table
	so that we can free memory if needed */
void* ht_remove(hashtable_t* hasht, char* key);


/* List keys. k should have length equals or greater than the number of keys */
void ht_list_keys(hashtable_t* hasht, char** k, size_t len);


/* 	List values. v should have length equals or greater 
	than the number of stored elements */
void ht_list_values(hashtable_t* hasht, void** v, size_t len);


/* Iterate through table's elements. */
hash_elem_t* ht_iterate(hash_elem_it* iterator);


/* Iterate through keys. */
char* ht_iterate_keys(hash_elem_it* iterator);


/* Iterate through values. */
void* ht_iterate_values(hash_elem_it* iterator);


/* 	Removes all elements stored in the hashtable.
	if free_data, all stored datas are also freed.*/
void ht_clear(hashtable_t* hasht, int free_data);


/* 	Destroy the hash table, and free memory.
	Data still stored are freed*/
void ht_destroy(hashtable_t* hasht);

#endif /* HASH_TABLE_H_ */