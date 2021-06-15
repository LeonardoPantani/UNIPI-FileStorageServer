/**
 * @file    communication.c
 * @brief   Contiene l'implementazione di alcune funzioni che permettono lo scambio di messaggi tra client e server.
 * @note    E' a più basso livello rispetto ai metodi di api.c, infatti vanno usate quelle per comunicare col server, mentre il server deve usare solamente queste per comunicare con il client.
 * @author  Leonardo Pantani
**/

#include "communication.h"

void setMessage(Message* msg, ActionType ac, int flags, char* path, void* data, size_t data_length) {
    msg->action = ac;

    msg->flags = flags;
    
    if(path != NULL) {
        msg->path_length = strlen(path);
    } else {
        msg->path_length = 0;
    }

    msg->path = path;


    msg->data_length = data_length;
    msg->data = data;
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
    int ret = read(fd, msg, sizeof(Message));
    checkM1(ret != sizeof(Message) && ret != 0, "lettura msg iniziale");

    #ifdef DEBUG_VERBOSE
        printf("Read message>> BYTE LETTI (iniziali) (azione %d): %d\n", msg->action, ret); fflush(stdout);
    #endif

    if(ret == 0) return 1; // connessione chiusa dall'altro partecipante (nessun byte letto)


    // leggo path dinamicamente (se è 0 la lunghezza non leggo nulla)
    if(msg->path_length > 0 && msg->path != NULL) {
        msg->path = malloc(msg->path_length+1); // +1 così valgrind è contento
        memset(msg->path, 0, msg->path_length+1); // +1 così valgrind è contento
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
    }

    // leggo data dinamicamente (se è 0 la lunghezza non leggo nulla)
    if(msg->data_length > 0 && msg->data != NULL) {
        msg->data = malloc(msg->data_length+1); // +1 così valgrind è contento
        memset(msg->data, 0, msg->data_length+1); // +1 così valgrind è contento
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
