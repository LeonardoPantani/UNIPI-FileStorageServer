/**
 * @file    list.c
 * @brief   Contiene l'implementazione della list dei client in attesa.
 * @author  Leonardo Pantani
**/

#include "list.h"

List* createList(int maxSlots) {
    checkNull(maxSlots <= 0, "numero di slot massimi minore o uguale a 0");
    List* lista = cmalloc(sizeof(List));
    lista->array = cmalloc(maxSlots*sizeof(int));

    lista->maxSlots = maxSlots;
    lista->usedSlots = 0;
    lista->first = 0;
    lista->last  = 0;
    
    return lista;
}


void deleteList(List *list) {
    if(list != NULL)
        free(list->array);
    free(list);
}


int addToList(List *list, int elemento) {
    checkM1(list == NULL, "add ad una list che non esiste");
    checkM1(list->usedSlots >= list->maxSlots, "add ad una list che ha raggiunto/superato il limite di slot massimi");
    list->usedSlots++;
    list->array[list->last] = elemento;
    list->last = (list->last+1) % list->maxSlots;

    return 0;
}


int removeFromList(List *list) {
    checkM1(list == NULL, "remove ad una list che non esiste");
    checkM1(list->usedSlots <= 0, "remove ad una list vuota"); 

    int preso;

    preso = list->array[list->first];
    list->usedSlots--;
    list->first = (list->first+1) % list->maxSlots;

    return preso;
}