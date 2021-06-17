/**
 * @file    slot.c
 * @brief   Contiene l'implementazione di uno slot della coda.
 * @author  Leonardo Pantani
**/

#include "slot.h"

Slot* createSlot(void* key, void* data) {
    Slot* slot = cmalloc(sizeof(Slot));

    slot->next = NULL;
    slot->key  = key;
    slot->data = data;

    return slot;
}

void deleteSlot(Slot* toClean, int clearData) {
    if(clearData == TRUE) {
        free(toClean->data);
    }
    
    free(toClean->key);
    free(toClean);
}