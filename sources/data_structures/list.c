/**
 * @file    list.c
 * @brief   Contiene l'implementazione della coda dei client in attesa.
 * @author  Leonardo Pantani
**/

#include "list.h"

List* createList(int maxClients) {
    checkNull(maxClients <= 0, "numero di client massimi minore o uguale a 0");
    List* lista = malloc(sizeof(List));
    checkNull(lista == NULL, "malloc lista client");

    lista->array = malloc(maxClients*sizeof(int));
    checkNull(lista->array == NULL, "malloc queue della lista client");

    lista->maxClients = maxClients;
    lista->numClients = 0;
    lista->first = 0;
    lista->last  = 0;
    
    return lista;
}


void deleteList(List *coda) {
    if(coda != NULL)
        free(coda->array);
    free(coda);
}


int addToList(List *coda, int elemento) {
    checkM1(coda == NULL, "add ad una coda che non esiste");
    checkM1(coda->numClients >= coda->maxClients, "add ad una coda che ha raggiunto/superato il limite di client massimi");
    coda->numClients++;
    coda->array[coda->last] = elemento;
    coda->last = coda->last+1;

    return 0;
}


int removeFromList(List *coda) {
    checkM1(coda == NULL, "remove ad una coda che non esiste");
    checkM1(coda->numClients <= 0, "remove ad una coda vuota"); 

    int preso;

    preso = coda->array[coda->first];
    coda->numClients--;
    coda->first = coda->first+1;

    return preso;
}