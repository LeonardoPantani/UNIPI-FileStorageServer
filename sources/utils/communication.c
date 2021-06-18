/**
 * @file    communication.c
 * @brief   Contiene l'implementazione di alcune funzioni che permettono lo scambio di messaggi tra client e server.
 * @note    E' a più basso livello rispetto ai metodi di api.c, infatti vanno usate quelle per comunicare col server, mentre il server deve usare solamente queste per comunicare con il client.
 * @author  Leonardo Pantani
**/

#include "communication.h"

void setMessage(Message* msg, ActionType ac, int flags, char* path, void* data, size_t data_length) {
    memset(msg, 0, sizeof(*msg));

    msg->action = ac;

    msg->flags = flags;

    if(path != NULL)
        strcpy(msg->path, path);

    msg->data_length = data_length;

    msg->data = data;
}

int sendMessage(int fd, Message* msg) {
    checkM1(msg == NULL, "messaggio nullo");
    
    // mando sempre tutto il messaggio (il body sarà vuoto)
    int ret = write(fd, msg, sizeof(Message));
    #ifdef DEBUG
        checkM1(ret != sizeof(Message), "scrittura msg iniziale");
    #else
        if(ret != sizeof(Message)) { // la comunicazione è fallita (non è stato possibile inviare tutti i dati)
            return -1;
        }
    #endif

    #ifdef DEBUG_VERBOSE
        printf("Send message>> BYTE SCRITTI (iniziali) (azione %d): %d\n", msg->action, ret); fflush(stdout);
    #endif

    // scrivo data dinamicamente (se è 0 la lunghezza non trasmetto niente)
    int remainingBytes = msg->data_length;
    char* writePointer = msg->data;
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

    // leggo data dinamicamente (se è 0 la lunghezza non leggo nulla)
    if(msg->data_length > 0 && msg->data != NULL) {
        msg->data = malloc(msg->data_length+1);
        memset(msg->data, 0, msg->data_length+1);
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

char* getRequestName(int code) {
    switch(code) {
        case 0 : return "UNKNOWN";
        case 12: return "OPEN";
        case 13: return "READ";
        case 14: return "READ_N";
        case 15: return "WRITE";
        case 16: return "APPEND";
        case 17: return "LOCK";
        case 18: return "UNLOCK";
        case 19: return "CLOSE";
        case 20: return "DELETE";

        default: return "?";
    }
}

char* getOperationName(int code) {
    switch(code) {
        case 0 : return "UNKNOWN";
        case 1: return "AC_WRITE_RECU";
        case 2: return "AC_WRITE_LIST";
        case 3: return "AC_READ_LIST";
        case 4: return "AC_READ_RECU";
        case 5: return "AC_ACQUIRE_MUTEX";
        case 6: return "AC_RELEASE_MUTEX";
        case 7: return "AC_DELETE";

        default: return "?";
    }
}