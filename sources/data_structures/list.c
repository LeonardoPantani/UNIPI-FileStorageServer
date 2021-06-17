/**
 * @file    list.c
 * @brief   Contiene l'implementazione della coda dei client in attesa.
 * @author  Leonardo Pantani
**/

#include "list.h"

void listInitialize(List* lista) {
    lista->testa = NULL;
    lista->coda = NULL;
    lista->usedSlots = 0;
}


void listDestroy(List *lista) {
    Slot* corrente = lista->testa;
    Slot* precedente = NULL;

    while(corrente != NULL) {
        precedente = corrente;
        corrente = corrente->next;

        deleteSlot(precedente, TRUE);
    }
}


int listEnqueue(List *lista, char* chiave, void* data) {
    Slot* nuovo = createSlot(chiave, data);

    if(lista->coda == NULL) {
        lista->testa = nuovo;
        lista->coda = lista->testa;
    } else {
        lista->coda->next = nuovo;
        lista->coda = lista->coda->next;
    }

    lista->usedSlots++;

    return 0;
}


int listPush(List* lista, char* chiave, void* data) {
    Slot* nuovo = createSlot(chiave, data);

    nuovo->next = lista->testa;
    lista->testa = nuovo;

    lista->usedSlots++;

    if(lista->usedSlots == 1) {
        lista->coda = lista->testa;
    }

    return 0;
}


void* listPop(List *lista) {
    Slot* corrente = lista->testa;
    void* data;

    if(lista->testa != NULL) {
        data = corrente->data;
        lista->testa = corrente->next;
        lista->usedSlots--;

        if(lista->usedSlots == 0) lista->coda = NULL;

        deleteSlot(corrente, FALSE);

        return data;
    }

    return NULL;
}


int listRemoveByKey(List* lista, char* chiave) {
    Slot* corrente = lista->testa;
    Slot* precedente = NULL;

    while(corrente != NULL && corrente->key && strcmp(corrente->key, chiave) != 0) {
        precedente = corrente;
        corrente = corrente->next;
    }

    if(corrente == NULL) { // non trovato
        return -1;
    }

    if(corrente != NULL && corrente == (lista->coda)) {
        lista->coda = precedente;
    }

    if(precedente == NULL) {
        lista->testa = lista->testa->next;
    } else {
        precedente->next = corrente->next;
    }

    deleteSlot(corrente, TRUE);
    lista->usedSlots--;

    return 0;
}


int listFind(List lista, char* chiave) {
    Slot* corrente = lista.testa;

    while(corrente != NULL) {
        if(corrente->key != NULL && strcmp(corrente->key, chiave) == 0) {
            return TRUE;
        }
        corrente = corrente->next;
    }

    return FALSE;
}


int listAdd(List* lista, int indice, char* chiave, void* data) {
    int i = 0;

    Slot* precedente = NULL;
    Slot* corrente = lista->testa;
    Slot* nuovo = createSlot(chiave, data);

    if(indice < 0 || indice > lista->usedSlots) {
        return -1;
    }

    if(indice == 0) {
        deleteSlot(nuovo, TRUE);
        return listPush(lista, chiave, data);
    } else if(indice == lista->usedSlots) {
        deleteSlot(nuovo, TRUE);
        return listEnqueue(lista, data, chiave);
    }

    while(i < indice) {
        precedente = corrente;
        corrente = corrente->next;
        i++;
    }

    precedente->next = nuovo;
    nuovo->next = corrente;

    lista->usedSlots++;

    return 0;
}


void* listGetByIndex(List lista, int indice) {
    int i = 0;

    Slot* corrente = lista.testa;

    if(indice < 0 || indice >= lista.usedSlots) {
        return NULL;
    }

    while(i < indice) {
        corrente = corrente->next;
        i++;
    }

    if(corrente != NULL) {
        return corrente->data;
    }

    return NULL;
}


int listRemoveByIndex(List* lista, int indice) {
    int i = 0;
    Slot* corrente = lista->testa;
    Slot* precedente = NULL;

    if(indice < 0 || indice >= lista->usedSlots) {
        return -1;
    } else if(indice == 0) {
        listPop(lista);
        return 0;
    }

    while(i < indice && i < lista->usedSlots) {
        precedente = corrente;
        corrente = corrente->next;
        i++;
    }

    if(indice == (lista->usedSlots - 1)) {
        lista->testa = precedente;
    }

    precedente->next = corrente->next;

    deleteSlot(corrente, TRUE);

    lista->usedSlots--;

    return 0;
}