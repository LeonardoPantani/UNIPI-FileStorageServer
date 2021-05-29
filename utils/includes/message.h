/**
 * @file    message.h
 * @brief   Contiene l'header delle funzioni che lavorano sui messaggi e la loro implementazione come strutture.
**/

#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <stdio.h>
#include <string.h>
#include <linux/limits.h>
#include "actions.h"

typedef struct {
    ActionType  action;
    int         flags;

    int         path_length;
    char*       path;

    int         data_length;
    char*       data;
} Message;


/**
 * @brief Imposta l'header del messaggio.
 * 
 * @param msg           Il messaggio di cui settare l'header
 * @param ac            Il tipo di azione riferita a quel messaggio
 * @param flags         Flags impostate da determinati messaggi
 * 
 * @param path          Percorso che identifica un file univocamente
 * @param data          Dati
 * @param data_length   Lunghezza dati
**/
static void setMessage(Message* msg, ActionType ac, int flags, char* path, void* data, size_t data_length);

#endif /* MESSAGE_H_ */