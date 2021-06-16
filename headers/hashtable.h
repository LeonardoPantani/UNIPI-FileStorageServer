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
	size_t data_length; // dimensione dati
    
    int lock; // identifica univocamente un client se ha bloccato il file (la relativa connessione) | -1 altrimenti
    List* codaRichiedentiLock;
    pthread_cond_t* rilascioLock;
	pthread_mutex_t* mutex;
    
    unsigned long updatedDate; // data in tempo unix dell'ultima modifica (o creazione)
	char path[MAX_PATH_LENGTH];
    char key[]; 	// Key of the stored element
} hash_elem_t;

//Hashtabe structure
typedef struct {
	unsigned int capacity;	// Hashtable capacity (in terms of hashed keys)
	unsigned int e_num;	// Number of element currently stored in the hashtable
	hash_elem_t** table;	// The table containing elements
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
	they are overwritten, and return by the function. Else it returns NULL.
	Returns HT_ERROR if there are memory alloc error */
void* ht_put(hashtable_t* hasht, char* key, void* data, size_t data_length, int lock, unsigned long updatedDate);


/* Retrieve path from the hashtable */
char* ht_getPath(hashtable_t* hasht, char* key);


/* 	Remove data from the hashtable */
void* ht_remove(hashtable_t* hasht, char* key);


/* Retrieve entire element from the hashtable */
hash_elem_t* ht_getElement(hashtable_t* hasht, char* key);


/* Iterate through table's elements. */
hash_elem_t* ht_iterate(hash_elem_it* iterator);


/* Iterate through keys. */
char* ht_iterate_keys(hash_elem_it* iterator);


/* Iterate through values. */
void* ht_iterate_values(hash_elem_it* iterator);


/* 	Removes all elements stored in the hashtable. */
void ht_clear(hashtable_t* hasht);


/* 	Destroy the hash table, and free memory.
	Data still stored are freed*/
void ht_destroy(hashtable_t* hasht);


/* Cerca una entry vecchia (per l'algoritmo di rimpiazzamento)
	e la restituisce */
hash_elem_t* ht_find_old_entry(hashtable_t* hasht);

#endif /* HASH_TABLE_H_ */