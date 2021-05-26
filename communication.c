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
    
    // mando sempre tutto il messaggio (le stringhe non saranno valide)
    int ret = write(fd, msg, sizeof(Message));
    checkM1(ret != sizeof(Message), "scrittura msg iniziale");

    #ifdef DEBUG_VERBOSE
        printf("Send message>> BYTE SCRITTI (iniziali) (azione %d): %d\n", msg->action, ret); fflush(stdout);
    #endif


    // scrivo path dinamicamente (se è 0 la lunghezza non trasmetto niente)
    int remainingBytes = msg->path_length;
    char* writePointer = msg->path;
    while(remainingBytes > 0) {
        ret = write(fd, writePointer, remainingBytes);
        checkM1(ret == -1, "scrittura fallita path");

        remainingBytes = remainingBytes - ret;
        writePointer = writePointer + ret;

        #ifdef DEBUG_VERBOSE
            printf("Send message>> BYTE SCRITTI (path) (azione %d): %d\n", msg->action, ret); fflush(stdout);
        #endif
    }

    // scrivo data dinamicamente (se è 0 la lunghezza non trasmetto niente)
    remainingBytes = msg->data_length;
    writePointer = msg->data;
    while(remainingBytes > 0) {
        ret = write(fd, writePointer, remainingBytes);
        checkM1(ret == -1, "scrittura fallita data");

        remainingBytes = remainingBytes - ret;
        writePointer = writePointer + ret;

        #ifdef DEBUG_VERBOSE
            printf("Send message>> BYTE SCRITTI (data) (azione %d): %d\n", msg->action, ret); fflush(stdout);
        #endif
    }

    #ifdef DEBUG_VERBOSE
        printf("=====================================\n"); fflush(stdout);
    #endif
    return 0;
}

int readMessage(int fd, Message* msg) {
    checkM1(msg == NULL, "msg nullo");

    // leggo tutto il messaggio (le stringhe non saranno valide)
    int l, ret;
    ret = read(fd, msg, sizeof(Message));
    checkM1(ret != sizeof(Message) && ret != 0, "lettura msg iniziale");

    #ifdef DEBUG_VERBOSE
        printf("Read message>> BYTE LETTI (iniziali) (azione %d): %d\n", msg->action, ret); fflush(stdout);
    #endif

    if(ret == 0) return 1; // connessione chiusa dall'altro partecipante (nessun byte letto)


    // leggo path dinamicamente (se è 0 la lunghezza non leggo nulla)
    if(msg->path_length > 0) {
        msg->path = malloc(msg->path_length);
        memset(msg->path, 0, msg->path_length);
        checkM1(msg->path == NULL, "malloc path");

        int remainingBytes = msg->path_length;
        char* writePointer = msg->path;
        while(remainingBytes > 0) {
            ret = read(fd, writePointer, remainingBytes);
            checkM1(ret == -1, "lettura fallita path");

            remainingBytes = remainingBytes - ret;
            writePointer = writePointer + ret;

            #ifdef DEBUG_VERBOSE
                printf("Read message>> BYTE LETTI (path) (azione %d): %d\n", msg->action, ret); fflush(stdout);
            #endif
        }
        msg->path[msg->path_length] = '\0';
    }

    // leggo data dinamicamente (se è 0 la lunghezza non leggo nulla)
    if(msg->data_length > 0) {
        msg->data = malloc(msg->data_length);
        memset(msg->data, 0, msg->data_length);
        checkM1(msg->data == NULL, "malloc data");

        int remainingBytes = msg->data_length;
        char* writePointer = msg->data;
        while(remainingBytes > 0) {
            ret = read(fd, writePointer, remainingBytes);
            checkM1(ret == -1, "lettura fallita data");

            remainingBytes = remainingBytes - ret;
            writePointer = writePointer + ret;

            #ifdef DEBUG_VERBOSE
                printf("Read message>> BYTE LETTI (data) (azione %d): %d\n", msg->action, ret); fflush(stdout);
            #endif
        }
    }

    #ifdef DEBUG_VERBOSE
        printf("=====================================\n"); fflush(stdout);
    #endif
    return 0;
}