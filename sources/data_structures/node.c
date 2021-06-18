/**
 * @file    node.c
 * @brief   Contiene l'implementazione di un nodo della coda.
 * @author  Leonardo Pantani
**/

#include "node.h"

Node* createNode(void* key, void* data) {
    Node* node = cmalloc(sizeof(Node));

    node->next = NULL;
    node->key  = key;
    node->data = data;

    return node;
}

void deleteNode(Node* toClean, int clearData) {
    if(clearData == TRUE) {
        free(toClean->data);
    }
    
    free(toClean->key);
    free(toClean);
}