/**
 * @file    communication.h
 * @brief   Contiene l'header delle funzioni usate per scambiare messaggi tra client e server.
 * @author  Leonardo Pantani
**/

#ifndef COMMUNICATION_H_
#define COMMUNICATION_H_

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/limits.h>

#include "utils.h"

typedef enum {
    /**
     *  RISPOSTE SERVER -> CLIENT
    **/
    // generiche
    ANS_ERROR               = -1,
    ANS_UNKNOWN             = 0,
    ANS_NO_PERMISSION       = 1,
    ANS_MAX_CONN_REACHED    = 2,
    ANS_OK                  = 3,
    ANS_BAD_RQST            = 4,

    // handshake server-client
    ANS_WELCOME             = 5,
    ANS_HELLO               = 6,

    // relative a file
    ANS_FILE_EXISTS         = 7,
    ANS_FILE_NOT_EXISTS     = 8,
    ANS_STREAM_START        = 9,
    ANS_STREAM_FILE         = 10,
    ANS_STREAM_END          = 11,

    /**
     * RICHIESTE CLIENT -> SERVER
    */
    REQ_OPEN                = 12,
    REQ_READ                = 13,
    REQ_READ_N              = 14,
    REQ_WRITE               = 15,
    REQ_APPEND              = 16,
    REQ_LOCK                = 17,
    REQ_UNLOCK              = 18,
    REQ_CLOSE               = 19,
    REQ_DELETE              = 20,

    /**
     * AZIONI DEL CLIENT (servono per identificare l'operazione da svolgere)
    **/
    AC_WRITE_RECU           = 1, // -w
    AC_WRITE_LIST           = 2, // -W
    AC_READ_LIST            = 3, // -r
    AC_READ_RECU            = 4, // -R
    AC_ACQUIRE_MUTEX        = 5, // -l
    AC_RELEASE_MUTEX        = 6, // -u
    AC_DELETE               = 7, // -c
} ActionType;

typedef struct {
    ActionType  action;
    int         flags;

    char        path[MAX_PATH_LENGTH];

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
void setMessage(Message* msg, ActionType ac, int flags, char* path, void* data, size_t data_length);

/**
 * @brief Invia un messaggio al descrittore file
 * 
 * @param fd    Descrittore a cui mandare il messaggio
 * @param msg   Il messaggio da mandare
 * 
 * @returns 0 in caso di successo, -1 in caso di fallimento
**/
int sendMessage(int fd, Message* msg);


/**
 * @brief Legge un header dal descrittore file
 * 
 * @param fd        Descrittore da cui ricevere l'header
 * @param msg   Puntatore su dove salvare l'header
 * 
 * @returns 0 in caso di successo, -1 in caso di fallimento
**/
int readMessage(int fd, Message* msg);

#endif /* COMMUNICATION_H_ */