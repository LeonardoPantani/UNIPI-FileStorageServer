/**
 * @file    list.h
 * @brief   Contiene l'header dei metodi utilizzati per la coda di client.
 * @author  Leonardo Pantani
**/

#ifndef LIST_H_
#define LIST_H_

#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

typedef struct {
    int first;
    int last;
    int *array;
    int maxClients;
    int numClients;
} List;

/**
 * @brief Crea una coda di al massimo numClients.
 * 
 * @param numClients    Numero di posti nella coda
 * 
 * @returns Puntatore alla coda creata, NULL in caso di errore
**/
List* createList(int numClients);

/**
 * @brief Elimina una coda di client.
 * 
 * @param coda  La coda su cui eseguire la free
**/
void deleteList(List *coda);

/**
 * @brief Aggiunge un elemento alla coda.
 * 
 * @param coda      La coda a cui aggiungere un elemento
 * @param elemento  L'elemento da aggiungere
 * 
 * @returns 1 in caso di successo, 0 in caso di fallimento
**/
int addToList(List *coda, int elemento);

/**
 * @brief Rimuove il primo elemento dalla coda
 * 
 * @param coda      La coda da cui rimuovere l'elemento
 * 
 * @returns 1 in caso di successo, 0 in caso di fallimento
**/
int removeFromList(List *coda);

#endif /* LIST_H_ */