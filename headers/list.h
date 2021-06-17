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
#include "slot.h"

typedef struct {
    Slot* testa;
    Slot* coda;
    int usedSlots;
} List;

void listInitialize(List* lista);
void listDestroy(List *lista);
int listEnqueue(List *lista, char* chiave, void* data);
int listPush(List* lista, char* chiave, void* data);
void* listPop(List *lista);
int listRemoveByKey(List* lista, char* chiave);
int listFind(List lista, char* chiave);
int listAdd(List* lista, int indice, char* chiave, void* data);
void* listGetByIndex(List lista, int indice);
int listRemoveByIndex(List* lista, int indice);

#endif /* LIST_H_ */