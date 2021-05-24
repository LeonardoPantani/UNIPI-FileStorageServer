/**
 * @file    message.h
 * @brief   Contiene l'header delle funzioni che lavorano sui messaggi e la loro implementazione come strutture.
**/

#ifndef MESSAGE_H_
#define MESSAGE_H_

#include "actions.h"

typedef struct {
    ActionType  action;
    int         flags;

    int         path_length;
    char*       abs_path;
} MessageHeader;


typedef struct {
    int     length;
    char*   buffer;
} MessageBody;


typedef struct {
    MessageHeader hdr;
    MessageBody   bdy;
} Message;


/**
 * @brief Imposta l'header del messaggio.
 * 
 * @param msg   Il messaggio di cui settare l'header
 * @param ac    Il tipo di azione riferita a quel messaggio
 * @param path  Percorso che identifica un file univocamente
 * @param flags Flags impostate da determinati messaggi
**/
static void setMessageHeader(Message* msg, ActionType ac, char* path, int flags);


/**
 * @brief Imposta il body del messaggio.
 * 
 * @param msg        Il messaggio di cui settare il body
 * @param bdy_length La dimensione del buffer
 * @param buffer     Buffer che contiene i dati
**/
static void setMessageBody(Message* msg, int bdy_length, char* buffer);

#endif /* MESSAGE_H_ */