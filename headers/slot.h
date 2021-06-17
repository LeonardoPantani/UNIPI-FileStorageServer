/**
 * @file    slot.h
 * @brief   Contiene l'header dei metodi utilizzati per definire il nodo di una lista.
 * @author  Leonardo Pantani
**/

#ifndef SLOT_H_
#define SLOT_H_

#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

typedef struct slot {
    struct slot* next;
    char *key;
    void* data;
} Slot;

/**
 * @brief Crea uno slot di una coda.
 * 
 * @param key   Chiave dello slot
 * @param data  Il contenuto dello slot
 * 
 * @returns Puntatore al nodo creato.
**/
Slot* createSlot(void* key, void* data);


/**
 * @brief Elimina un nodo.
 * 
 * @param toClean   Il nodo da eliminare
 * @param clearData Specifica se si deve fare la free su data o no
 * 
**/
void deleteSlot(Slot* toClean, int clearData);

#endif /* SLOT_H_ */