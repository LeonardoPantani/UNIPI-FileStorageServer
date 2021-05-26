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
#include <libgen.h>

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
    if(flags != 1 && flags != 2 && flags != 3) { // le flag passate non sono valide
        errno = EINVAL;
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
    extern char ejectedFileFolder[PATH_MAX];

    if((socketConnection = searchAssocByName(socketPath)) != -1) {
        FILE* file = fopen(pathname, "rb");
        if(!file) { // file non apribile
            errno = ENOENT;
            return -1;
        }

        struct stat sb;
        if(stat(pathname, &sb) == -1) { // impossibile aprire statistiche file
            errno = EBADFD;
            return -1;
        }

        char *buffer = malloc(sb.st_size);
        fread(buffer, sb.st_size, 1, file);

        setMessageBody(messaggio, strlen(buffer), buffer);

        if(sendMessage(socketConnection, messaggio) == 0) {
            // una volta inviata la richiesta di check del file
            int esito = readMessageHeader(socketConnection, header);
            if(esito == 0) {
                int esito2 = readMessageBody(socketConnection, body);
                switch(header->action) {
                    case AC_FILERCVD: {
                        ppf(CLR_HIGHLIGHT); printf("CLIENT> File '%s' caricato!\n", pathname); ppff();
                        break;
                    }
                    
                    case AC_FLUSH_START: {
                        esito = readMessageHeader(socketConnection, header);
                        while(esito == 0 && header->action != AC_FLUSH_END) {
                            esito2 = readMessageBody(socketConnection, body);
                            if(esito2 == 0) {
                                ppf(CLR_HIGHLIGHT); printf("CLIENT> Espulso file '%s' per fare spazio a %s\n", header->abs_path, pathname); ppff();
                                if(strcmp(ejectedFileFolder, "#") == 0) {
                                    ppf(CLR_WARNING); printf("CLIENT> Il file '%s' non verrà salvato perché non è stata specificata nessuna cartella con -D.\n", header->abs_path); ppff();
                                } else {
                                    FILE* filePointer;
                                    char percorso[PATH_MAX];
                                    memset(percorso, 0, strlen(percorso));

                                    strcpy(percorso, ejectedFileFolder);
                                    
                                    if(percorso[strlen(percorso) - 1] != '/') {
                                        strcat(percorso, "/");
                                    }
                                    strcat(percorso, basename(header->abs_path));

                                    filePointer = fopen(percorso, "wb");
                                    if(filePointer == NULL) { pe("File non aperto!!"); }

                                    fwrite(body->buffer, 1, body->length, filePointer);
                                    ppf(CLR_INFO); printf("CLIENT> File '%s' salvato in '%s'\n", header->abs_path, percorso); ppff();
                                    
                                    fclose(filePointer);
                                }
                            }
                            esito = readMessageHeader(socketConnection, header);
                        }
                        esito2 = readMessageBody(socketConnection, body); // da ignorare malamente

                        
                        esito = readMessageHeader(socketConnection, header);
                        if(esito == 0) {
                            esito2 = readMessageBody(socketConnection, body);
                            if(header->action == AC_FILERCVD) {
                                ppf(CLR_HIGHLIGHT); printf("CLIENT> File '%s' caricato!\n", pathname); ppff();
                            }
                        }
                            
                        break;
                    }

                    case AC_FILEEXISTS: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' esiste già, non puoi fare una CREATE.\n", pathname); ppff();
                        errno = EEXIST;
                        return -1;
                    }

                    case AC_FILENOTEXISTS: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' non esiste, devi fare prima una CREATE.\n", pathname); ppff();
                        errno = ENOENT;
                        return -1;
                    }

                    case AC_MAXFILESREACHED: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' non può essere salvato perché il server ha raggiunto il numero massimo di file salvati.\n", pathname); ppff();
                        errno = ENOSPC;
                        return -1;
                    }

                    case AC_NOSPACELEFT: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' non può essere salvato perché il server non avrebbe più spazio disponibile in memoria.\n", pathname); ppff();
                        errno = ENOSPC;
                        return -1;
                    }

                    default: {
                        ppf(CLR_ERROR); printf("CLIENT> Il server ha mandato una risposta non valida: %s", body->buffer); ppff();
                        errno = EBADRQC;
                        return -1;
                    }
                }
            }  else { // errore risposta
                stampaDebug("Errore body richiesta visione file!");
                errno = EBADMSG;
                return -1;
            }
        } else { // errore invio messaggio
            stampaDebug("Impossibile inviare la richiesta di visione file!");
            errno = EBADE;
            return -1;
        }


        return 0;
    } else { // non c'è un socket su cui comunicare
        errno = EPIPE;
        return -1;
    }
}


/**
 * @brief Legge il contenuto del file dal server (se esiste).
 * 
 * @param pathname percorso relativo al file che si vuole leggere
 * @param buf      puntatore sull'heap del file ottenuto
 * @param size     dimensione del buffer dati ottenuto
 * 
 * @returns 0 in caso di successo, -1 in caso di fallimento (setta errno)
**/
int readFile(const char* pathname, void** buf, size_t* size) {
    // creo variabili per contenere richieste
    MessageHeader* header = malloc(sizeof(MessageHeader));
    checkStop(header == NULL, "malloc header");

    MessageBody* body = malloc(sizeof(MessageBody));
    checkStop(body == NULL, "malloc body");

    Message* messaggio = malloc(sizeof(Message));
    checkStop(messaggio == NULL, "malloc risposta");
    // fine variabili per contenere richieste

    setMessageHeader(messaggio, AC_READ, (char*)pathname, 0);
    setMessageBody(messaggio, 0, NULL);

    int socketConnection;

    if((socketConnection = searchAssocByName(socketPath)) != -1) {
        if(sendMessage(socketConnection, messaggio) == 0) {
            // una volta inviata la richiesta di check del file
            int esito = readMessageHeader(socketConnection, header);
            if(esito == 0) {
                int esito2 = readMessageBody(socketConnection, body);

                switch(header->action) {
                    case AC_FILESVD: {
                        if(esito2 == 0) {
                            *buf = body->buffer;
                            *size = body->length;
                            ppf(CLR_INFO); printf("CLIENT> File '%s' salvato in memoria all'indirizzo %p.\n", pathname, &body->buffer); ppff();
                        }
                        
                        break;
                    }

                    case AC_FILENOTEXISTS: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' non esiste, non può essere letto\n", pathname); ppff();
                        errno = ENOENT;
                        return -1;
                    }

                    default: {
                        ppf(CLR_ERROR); printf("CLIENT> Il server ha mandato una risposta non valida: %s", body->buffer); ppff();
                        errno = EBADRQC;
                        return -1;
                    }
                }
            }  else { // errore risposta
                stampaDebug("Errore lettura header richiesta lettura file.");
                errno = EBADMSG;
                return -1;
            }
        } else { // errore invio messaggio
            stampaDebug("Impossibile inviare la richiesta di lettura file.");
            errno = EBADE;
            return -1;
        }

        return 0;
    } else { // non c'è un socket su cui comunicare
        errno = EPIPE;
        return -1;
    }
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