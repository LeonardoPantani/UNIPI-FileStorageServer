/**
 * @file    node.h
 * @brief   Contiene l'header dei metodi utilizzati per definire il nodo di una lista.
 * @author  Leonardo Pantani
**/

#ifndef NODE_H_
#define NODE_H_

#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

/**
 * @brief   Struttura che implementa un nodo di una lista. Non implementata.
 * 
 * @struct  next    Il nodo successivo
 * @struct  key     La chiave relativa al nodo
 * @struct  data    Il contenuto del nodo
**/
typedef struct node {
    struct node* next;
    char *key;
    void* data;
} Node;

/**
 * @brief Crea uno node di una coda.
 * 
 * @param key   Chiave dello node
 * @param data  Il contenuto dello node
 * 
 * @returns Puntatore al nodo creato.
**/
Node* createNode(void* key, void* data);


/**
 * @brief Elimina un nodo.
 * 
 * @param toClean   Il nodo da eliminare
 * @param clearData Specifica se si deve fare la free su data o no
 * 
**/
void deleteNode(Node* toClean, int clearData);

#endif /* NODE_H_ */