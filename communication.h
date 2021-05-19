/**
 * @file    communication.c
 * @brief   Contiene l'header delle funzioni usate per scambiare messaggi tra client e server.
**/
#ifndef COMMUNICATION_H_
#define COMMUNICATION_H_

#include "utils/includes/message.h"

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