/**
 * @file    list.h
 * @brief   Contiene l'header dei metodi della struttura dati list.
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
    int maxSlots;
    int usedSlots;
} List;

/**
 * @brief Crea una lista di al massimo usedSlots
 * 
 * @param usedSlots    Numero di posti nella lista
 * 
 * @returns Puntatore alla lista creata, NULL in caso di errore
**/
List* createList(int usedSlots);

/**
 * @brief Elimina una lista
 * 
 * @param list  La lista su cui eseguire la free
**/
void deleteList(List *list);

/**
 * @brief Aggiunge un elemento alla lista
 * 
 * @param list      La lista a cui aggiungere un elemento
 * @param elemento  L'elemento da aggiungere
 * 
 * @returns 0 in caso di successo, -1 in caso di fallimento
**/
int addToList(List *list, int elemento);

/**
 * @brief Rimuove il primo elemento dalla lista
 * 
 * @param list  La lista da cui rimuovere l'elemento
 * 
 * @returns L'elemento rimosso dalla lista
**/
int removeFromList(List *list);

#endif /* LIST_H_ */