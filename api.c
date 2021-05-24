/**
 * @file    api.c
 * @brief   Contiene l'implementazione delle funzioni dell'api
 *          che permette al client di collegarsi al server.
**/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/stat.h>

#include "utils/includes/macro.h"
#include "utils/includes/utils.h"
#include "communication.h"
#include "api.h"

#define O_CREATE    1
#define O_LOCK      2

int openConnection(const char* sockname, int msec, const struct timespec abstime) {
    // creazione socket
    int socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    checkM1(socket_fd == -1, "creazione socket client");

    struct sockaddr_un address;
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, sockname, strlen(sockname) + sizeof(char));

    int esito;

    struct timespec tempoInizio, tempoAttuale;
    clock_gettime(CLOCK_MONOTONIC, &tempoInizio);
    struct timespec t;
    t.tv_sec  = (int)msec/1000;
    t.tv_nsec = (msec%1000)*1000000;

    while(TRUE) {
        clock_gettime(CLOCK_MONOTONIC, &tempoAttuale);
        if(timespecDiff(tempoInizio, tempoAttuale, abstime)) {
            break;
        }

        stampaDebug("CLIENT> Connessione in corso...");
        errno = 0;
        if(connect(socket_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
            // connessione fallita
            #ifdef DEBUG
                pe("CLIENT> Connessione fallita");
            #endif
        } else {
            stampaDebug("CLIENT> Connessione effettuata.");
            setSocketAssociation(socket_fd, (char*)sockname);
            return socket_fd;
        }

        nanosleep(&t, NULL);
    }

    return -1;
}


int closeConnection(const char* sockname) {
    int fd;
    if((fd = searchAssocByName((char*)sockname)) != -1) {
        close(fd);
        return 0;
    } else {
        return -1;
    }
}


int openFile(const char* pathname, int flags) { // O_CREATE = 1 | O_LOCK = 2 | O_CREATE && O_LOCK = 3
    if(flags != 1 && flags != 2 && flags != 3) {
        return -1;
    }
    // creo variabili per contenere richieste
    MessageHeader* header = malloc(sizeof(MessageHeader));
    checkStop(header == NULL, "malloc header");

    MessageBody* body = malloc(sizeof(MessageBody));
    checkStop(body == NULL, "malloc body");

    Message* messaggio = malloc(sizeof(Message));
    checkStop(messaggio == NULL, "malloc risposta");
    // fine variabili per contenere richieste

    setMessageHeader(messaggio, AC_OPEN, (char*)pathname, flags);
    setMessageBody(messaggio, 0, NULL);

    int socketConnection;

    if((socketConnection = searchAssocByName(socketPath)) != -1) {
        FILE* file = fopen(pathname, "rb");
        checkM1(!file, "file non apribile");

        struct stat sb;
        checkM1(stat(pathname, &sb) == -1, "lettura statistiche file");


        char *buffer = malloc(sb.st_size);
        fread(buffer, sb.st_size, 1, file);

        setMessageBody(messaggio, strlen(buffer), buffer);

        if(sendMessage(socketConnection, messaggio) == 0) {
            // una volta inviata la richiesta di check del file
            int esito = readMessageHeader(socketConnection, header);
            if(esito == 0) {
                switch(header->action) {
                    case AC_FILERCVD: {
                        ppf(CLR_HIGHLIGHT); printf("CLIENT> File '%s' caricato!", pathname); ppff();
                        break;
                    }

                    case AC_FILEEXISTS: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' esiste già, non puoi fare una CREATE.", pathname); ppff();
                        return -1;

                        break;
                    }

                    case AC_FILENOTEXISTS: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' non esiste, devi fare prima una CREATE.", pathname); ppff();
                        return -1;

                        break;
                    }

                    default: {
                        ppf(CLR_ERROR); printf("CLIENT> Risposta dal server non valida!"); ppff();
                        return -1;
                        
                        break;
                    }
                }
            }  else { // errore risposta
                stampaDebug("errore risposta");
                return -1;
            }
        } else {
            stampaDebug("Impossibile inviare la richiesta di visione file!");
        }

        return 0;
    } else { // non c'è un socket su cui comunicare
        errno = EPIPE;
        return -1;
    }
}


int readFile(const char* pathname, void** buf, size_t* size) {
    return -1;
}


int writeFile(const char* pathname, const char* dirname) {
    return -1;
}


int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname) {
    return -1;
}


int lockFile(const char* pathname) {
    return -1;
}


int unlockFile(const char* pathname) {
    return -1;
}


int closeFile(const char* pathname) {
    return -1;
}


int removeFile(const char* pathname) {
    return -1;
}