/**
 * @file    communication.c
 * @brief   Contiene l'implementazione di alcune funzioni che permettono lo scambio di messaggi tra client e server.
 * 
 * NOTA: E' a più basso livello rispetto ai metodi di api.c, infatti vanno usate quelle per comunicare col server...
 * ... mentre il server deve usare solamente queste per comunicare con il client.
**/
#include <stdio.h>
#include <unistd.h>

#include "utils/includes/macro.h"
#include "utils/includes/utils.h"
#include "communication.h"

int setSocketAssociation(int fd, char* socketname) {
    if(sockAssocArrIter >= 10) return -1;

    SocketAssociation ass;
    ass.fd = fd;
    ass.socketname = socketname;

    sockAssocArr[sockAssocArrIter] = ass;
    sockAssocArrIter++;

    return 0;
}

int removeSocketAssociation(char* socketname) {
    for(int i = 0; i < sockAssocArrIter; i++) {
        if(strcmp(sockAssocArr[i].socketname, socketname) == 0) {
            sockAssocArr->fd = -1;
            sockAssocArr->socketname = NULL;
            sockAssocArrIter = i;
            return 0;
        }
    }

    return -1;
}

int searchAssocByName(char* socketname) {
    for(int i = 0; i < sockAssocArrIter; i++) {
        if(strcmp(sockAssocArr[i].socketname, socketname) == 0) {
            return sockAssocArr[i].fd;
        }
    }

    return -1;
}

int sendMessage(int fd, Message* msg) {
    checkM1(msg == NULL, "messaggio nullo");
    
    // mando sempre almeno l'header
    int ret = write(fd, &(msg->hdr), sizeof(MessageHeader));
    checkM1(ret != sizeof(MessageHeader), "scrittura fallita header");

    // scrivo lunghezza body
    ret = write(fd, &(msg->bdy.length), sizeof(int));
    checkM1(ret != sizeof(int), "scrittura fallita dimensione testo");

    // se la lunghezza del messaggio è 0 o il buffer è NULL allora ho finito
    if(msg->bdy.length == 0 || msg->bdy.buffer == NULL) {
        return 0;
    }

    // scrivo dati dinamicamente
    int remainingBytes = msg->bdy.length;
    char* writePointer = msg->bdy.buffer;
    while(remainingBytes > 0) {
        ret = write(fd, writePointer, remainingBytes);
        checkM1(ret == -1, "scrittura fallita body");

        remainingBytes = remainingBytes - ret;
        writePointer = writePointer + ret;
    }
    return 0;
}

int readMessageHeader(int fd, MessageHeader* msg_hdr) {
    checkM1(msg_hdr == NULL, "hdr nullo");

    int ret = read(fd, msg_hdr, sizeof(MessageHeader));
    if(ret == 0) {
        return 1; // connessione chiusa dall'altro partecipante (nessun byte letto)
    }

    // leggo l'header del messaggio
    printf("Dimensione msg: %d\n", ret); fflush(stdout);
    checkM1(ret != sizeof(MessageHeader), "lettura header"); 
    return 0;
}

int readMessageBody(int fd, MessageBody* msg_bdy) {
    checkM1(msg_bdy == NULL, "bdy nullo");

    // effettuo 2 read ora: una per leggere la lunghezza del buffer e poi il buffer stesso
    // 1. lettura lunghezza buffer
    int l, ret;
    ret = read(fd, &l, sizeof(int));
    checkM1(ret != sizeof(int), "lettura lunghezza buffer");
    msg_bdy->length = l;

    if(l == 0) return -2; // se la lunghezza del body è 0 restituisco -2 (differenziato rispetto a -1 = errore)

    // 2. lettura buffer
    msg_bdy->buffer = malloc(l);
    checkM1(msg_bdy->buffer == NULL, "malloc buffer");


    // leggo dati dinamicamente
    int remainingBytes = l;
    char* writePointer = msg_bdy->buffer;
    while(remainingBytes > 0) {
        ret = read(fd, writePointer, remainingBytes);
        checkM1(ret == -1, "lettura fallita body");

        remainingBytes = remainingBytes - ret;
        writePointer = writePointer + ret;
    }
    return 0;
}