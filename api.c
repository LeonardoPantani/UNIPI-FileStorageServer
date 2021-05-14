/**
 * @file    api.c
 * @brief   Contiene l'implementazione delle funzioni dell'api
 *          che permette al client di collegarsi al server.
**/
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "utils/includes/utils.h"
#include "utils/includes/macro.h"

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
            return socket_fd;
        }

        nanosleep(&t, NULL);
    }

    return -1;
}


int closeConnection(const char* sockname) {
    return -1;
}


int openFile(const char* pathname, int flags) {
    return -1;
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