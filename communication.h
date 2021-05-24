/**
 * @file    communication.c
 * @brief   Contiene l'header delle funzioni usate per scambiare messaggi tra client e server.
**/
#ifndef COMMUNICATION_H_
#define COMMUNICATION_H_

#include "utils/includes/message.h"

typedef struct SocketAssociation {
    int fd;
    char* socketname;
} SocketAssociation;

SocketAssociation sockAssocArr[10];
int sockAssocArrIter = 0;

char socketPath[4096]; // nome del socket

/**
 * @brief Salva un'associazione del socket al descrittore file.
 * 
 * @param fd            Il file descriptor
 * @param socketname    Il nome del socket da collegargli
 * 
 * @returns 0 in caso di successo, -1 in caso di fallimento
**/
int setSocketAssociation(int fd, char* socketname);

/**
 * @brief Rimuove un'associazione del socket al descrittore file.
 * 
 * @param fd            Il file descriptor
 * @param socketname    Il nome del socket da collegargli
 * 
 * @returns 0 in caso di successo, -1 in caso di fallimento
**/
int removeSocketAssociation(char* socketname);

/**
 * @brief Trova il file descriptor dal socket name.
 * 
 * @param socketname    Il nome del socket da cui ricavare il fd
 * 
 * @returns fd associato in caso di successo, -1 in caso di fallimento
**/
int searchAssocByName(char* socketname);

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
 * @param msg_hdr   Puntatore su dove salvare l'header
 * 
 * @returns 0 in caso di successo, -1 in caso di fallimento
**/
int readMessageHeader(int fd, MessageHeader* msg_hdr);

/**
 * @brief Legge il body dal descrittore file
 * 
 * @param fd        Descrittore da cui ricevere il body
 * @param msg_bdy   Puntatore su dove salvare il body
 * 
 * @returns 0 in caso di successo, -1 in caso di fallimento
**/
int readMessageBody(int fd, MessageBody* msg_bdy);

#endif /* COMMUNICATION_H_ */