/**
 * @file    api.c
 * @brief   Contiene l'implementazione delle funzioni dell'api che permette al client di collegarsi al server.
 * @author  Leonardo Pantani
**/

#include "api.h"

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

int openConnection(const char* sockname, int msec, const struct timespec abstime) {
    // creazione socket
    int socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    checkM1(socket_fd == -1, "creazione socket client");

    // associazione indirizzo
    struct sockaddr_un address;
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, sockname, strlen(sockname) + sizeof(char));

    // differenza tempo
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

        printf("CLIENT> Connessione in corso...\n"); fflush(stdout);
        errno = 0;
        if(connect(socket_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
            printf("CLIENT> Connessione fallita.\n"); fflush(stdout);
        } else {
            printf("CLIENT> Connessione effettuata.\n"); fflush(stdout);
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
    
    // creo variabile per contenere richieste
    Message* msg = calloc(4, sizeof(Message));
    checkStop(msg == NULL, "malloc risposta");
    // fine variabile per contenere richieste

    int socketConnection;
    extern char ejectedFileFolder[PATH_MAX];

    if((socketConnection = searchAssocByName(socketPath)) != -1) {
        setMessage(msg, REQ_OPEN, flags, (char*)pathname, NULL, 0);

        if(sendMessage(socketConnection, msg) == 0) {
            if(readMessage(socketConnection, msg) == 0) {
                switch(msg->action) {
                    case ANS_OK: {
                        printSave("OF CLIENT> File '%s' aperto!", pathname);
                        if(flags == 2 || flags == 3) {
                            ppf(CLR_IMPORTANT); printSave("OF CLIENT> File '%s' lockato!", pathname); ppff();
                        }
                        break;
                    }

                    case ANS_STREAM_START: {
                        while(readMessage(socketConnection, msg) == 0 && msg->action != ANS_STREAM_END) {
                            ppf(CLR_HIGHLIGHT); printSave("OF CLIENT> Espulso file remoto '%s' (%d bytes) per fare spazio a %s", msg->path, msg->data_length, pathname); ppff();
                            if(strcmp(ejectedFileFolder, "#") == 0) {
                                ppf(CLR_WARNING); printSave("OF CLIENT> Il file remoto '%s' (%d bytes) non verrà salvato perché non è stata specificata nessuna cartella con -D.", msg->path, msg->data_length); ppff();
                            } else {
                                FILE* filePointer;
                                char percorso[PATH_MAX];
                                memset(percorso, 0, PATH_MAX);

                                strcpy(percorso, ejectedFileFolder);
                                
                                if(percorso[PATH_MAX - 1] != '/') {
                                    strcat(percorso, "/");
                                }
                                strcat(percorso, basename(msg->path));

                                filePointer = fopen(percorso, "wb");
                                if(filePointer == NULL) { 
                                    ppf(CLR_ERROR); printSave("OF CLIENT> Impossibile salvare file remoto nel percorso specificato: '%s'.", percorso);
                                    continue;
                                }

                                fwrite(msg->data, 1, msg->data_length, filePointer);
                                ppf(CLR_INFO); printSave("OF CLIENT> File remoto '%s' (%d bytes) salvato in '%s'", msg->path, msg->data_length, percorso); ppff();
                                
                                fclose(filePointer);
                                // free(msg->path); // FIXME
                                free(msg->data);
                            }
                        }
                        
                        if(readMessage(socketConnection, msg) == 0) {
                            if(msg->action == ANS_OK) {
                                printSave("OF CLIENT> File '%s' aperto!", pathname);
                                if(flags == 2 || flags == 3) {
                                    ppf(CLR_IMPORTANT); printSave("OF CLIENT> File '%s' lockato!", pathname); ppff();
                                }
                            } else {
                                ppf(CLR_ERROR); printSave("OF CLIENT> Il server ha mandato una risposta non valida (2). ACTION: %d", msg->action); ppff();
                                errno = EBADRQC;
                            }
                        }
                            
                        break;
                    }

                    case ANS_BAD_RQST: {
                        ppf(CLR_ERROR); printSave("OF CLIENT> Richiesta errata!"); ppff();
                        errno = EBADRQC;
                        break;
                    }

                    case ANS_FILE_EXISTS: {
                        ppf(CLR_ERROR); printSave("OF CLIENT> Il file '%s' esiste già, non puoi fare una CREATE.", pathname); ppff();
                        errno = EEXIST;
                        break;
                    }

                    case ANS_FILE_NOT_EXISTS: {
                        ppf(CLR_ERROR); printSave("OF CLIENT> Il file '%s' non esiste, devi fare prima una CREATE.", pathname); ppff();
                        errno = ENOENT;
                        break;
                    }
                    
                    default: {
                        ppf(CLR_ERROR); printSave("OF CLIENT> Il server ha mandato una risposta non valida. ACTION: %d", msg->action); ppff();
                        errno = EBADRQC;
                    }
                }
            } else { // il server non ha risposto al client
                printSave("OF CLIENT> Il server non ha risposto alla richiesta. ACTION: %d", msg->action);
                errno = EBADMSG;
            }
        } else { // impossibile inviare la richiesta
            printSave("OF CLIENT> Non è stato possibile inviare la richiesta. ACTION: %d", msg->action);
            errno = EBADE;
        }

        if(msg->action == ANS_OK || msg->action == ANS_STREAM_END) {
            free(msg);
            return 0;
        } else {
            free(msg);
            return -1;
        }
    } else { // non c'è un socket su cui comunicare
        errno = EPIPE;
        return -1;
    }
}


int readFile(const char* pathname, void** buf, size_t* size) {
    // creo variabile per contenere richieste
    Message* msg = calloc(4, sizeof(Message));
    checkStop(msg == NULL, "malloc risposta");
    // fine variabile per contenere richieste

    setMessage(msg, REQ_READ, 0, (char*)pathname, NULL, 0); // inizializzo

    int socketConnection;

    if((socketConnection = searchAssocByName(socketPath)) != -1) {
        if(sendMessage(socketConnection, msg) == 0) {
            if(readMessage(socketConnection, msg) == 0) { // se l'esito della richiesta è 0 allora ok, altrimenti c'è stato un problema
                switch(msg->action) {
                    case ANS_OK: {
                        *buf = msg->data;
                        *size = msg->data_length;
                        ppf(CLR_INFO); printSave("RF CLIENT> File '%s' salvato in memoria.", pathname); ppff();
                        break;
                    }

                    case ANS_NO_PERMISSION: {
                        ppf(CLR_ERROR); printSave("RF CLIENT> Il file '%s' non può essere letto perché è lockato da un altro client.", pathname); ppff();
                        errno = EACCES;
                        break;
                    }

                    case ANS_FILE_NOT_EXISTS: {
                        ppf(CLR_ERROR); printSave("RF CLIENT> Il file '%s' non esiste, non può essere letto.", pathname); ppff();
                        errno = ENOENT;
                        break;
                    }

                    default: {
                        ppf(CLR_ERROR); printSave("RF CLIENT> Il server ha mandato una risposta non valida. ACTION: %d", msg->action); ppff();
                        errno = EBADRQC;
                    }
                }
            } else { // il server non ha risposto al client
                printSave("RF CLIENT> Il server non ha risposto alla richiesta. ACTION: %d", msg->action);
                errno = EBADMSG;
            }
        } else { // impossibile inviare la richiesta
            printSave("RF CLIENT> Non è stato possibile inviare la richiesta. ACTION: %d", msg->action);
            errno = EBADE;
        }

        if(msg->action == ANS_OK) {
            // free(msg->path); // FIXME
            free(msg);
            return 0;
        } else {
            free(msg);
            return -1;
        }
    } else { // non c'è un socket su cui comunicare
        errno = EPIPE;
        return -1;
    }
}


int readNFiles(int N, const char* dirname) {
    // creo variabile per contenere richieste
    Message* msg = calloc(4, sizeof(Message));
    checkStop(msg == NULL, "malloc risposta");
    // fine variabile per contenere richieste

    if(N <= 0) N = -1; // se N <= 0 allora vanno letti tutti i file, lo imposto a -1 così il server capisce cosa intendo

    setMessage(msg, REQ_READ_N, 0, NULL, &N, sizeof(int)); // inizializzo

    int socketConnection;
    extern char savedFileFolder[PATH_MAX];

    if((socketConnection = searchAssocByName(socketPath)) != -1) {
        if(sendMessage(socketConnection, msg) == 0) {
            if(readMessage(socketConnection, msg) == 0) {
                switch(msg->action) {
                    case ANS_STREAM_START: {
                        while(readMessage(socketConnection, msg) == 0 && msg->action != ANS_STREAM_END) {
                            ppf(CLR_HIGHLIGHT); printSave("RN CLIENT> File remoto '%s' ricevuto dal server.", msg->path); ppff();
                            if(strcmp(savedFileFolder, "#") == 0) {
                                ppf(CLR_WARNING); printSave("RN CLIENT> Il file remoto '%s' (%d bytes) non verrà salvato perché non è stata specificata nessuna cartella con -d.", msg->path, msg->data_length); ppff();
                            } else {
                                FILE* filePointer;
                                char percorso[PATH_MAX];
                                memset(percorso, 0, PATH_MAX);

                                strcpy(percorso, savedFileFolder);
                                
                                if(percorso[PATH_MAX - 1] != '/') {
                                    strcat(percorso, "/");
                                }
                                strcat(percorso, basename(msg->path));

                                filePointer = fopen(percorso, "wb");
                                if(filePointer == NULL) { 
                                    ppf(CLR_ERROR); printSave("RN CLIENT> Impossibile salvare file remoto nel percorso specificato: '%s'.", percorso);
                                    continue;
                                }

                                fwrite(msg->data, 1, msg->data_length, filePointer);
                                ppf(CLR_INFO); printSave("RN CLIENT> File remoto '%s' (%d bytes) salvato in '%s'", msg->path, msg->data_length, percorso); ppff();
                                
                                fclose(filePointer);
                                // free(msg->path); // FIXME
                                free(msg->data);
                            }
                        }
                        break;
                    }

                    case ANS_FILE_NOT_EXISTS: { // non ci sono file da mandare
                        ppf(CLR_ERROR); printSave("RN CLIENT> Il server non possiede file da inviarti."); ppff();
                        errno = ENOENT;
                        break;
                    }

                    default: {
                        ppf(CLR_ERROR); printSave("RN CLIENT> Il server ha mandato una risposta non valida. ACTION: %d", msg->action); ppff();
                        errno = EBADRQC;
                    }
                }
            } else { // il server non ha risposto al client
                printSave("RN CLIENT> Il server non ha risposto alla richiesta. ACTION: %d", msg->action);
                errno = EBADMSG;
            }
        } else { // impossibile inviare la richiesta
            printSave("RN CLIENT> Non è stato possibile inviare la richiesta. ACTION: %d", msg->action);
            errno = EBADE;
        }

        if(msg->action == ANS_STREAM_END) {
            free(msg);
            return 0;
        } else {
            free(msg);
            return -1;
        }
    } else { // non c'è un socket su cui comunicare
        errno = EPIPE;
        return -1;
    }
}


int writeFile(const char* pathname, const char* dirname) {
    // creo variabile per contenere richieste
    Message* msg = calloc(4, sizeof(Message));
    checkStop(msg == NULL, "malloc risposta");
    // fine variabile per contenere richieste

    setMessage(msg, ANS_UNKNOWN, 0, NULL, NULL, 0); // inizializzo

    int socketConnection;

    if((socketConnection = searchAssocByName(socketPath)) != -1) {
        FILE* file = fopen(pathname, "rb");
        if(!file) { // file non apribile
            errno = ENOENT;
            free(msg);
            return -1;
        }
        struct stat sb;
        if(stat(pathname, &sb) == -1) { // impossibile aprire statistiche file
            errno = EBADF;
            free(msg);
            return -1;
        }
        void *buffer = cmalloc(sb.st_size);
        if(fread(buffer, 1, sb.st_size, file) != sb.st_size) {
            errno = EIO;
            free(msg);
            free(buffer);
            return -1;
        }
        fclose(file);

        setMessage(msg, REQ_WRITE, 0, (char*)pathname, buffer, sb.st_size);

        if(sendMessage(socketConnection, msg) == 0) {
            free(buffer);
            if(readMessage(socketConnection, msg) == 0) {
                switch(msg->action) {
                    case ANS_OK: {
                        printSave("WF CLIENT> File '%s' (%ld bytes) caricato!", pathname, sb.st_size);
                        break;
                    }
                    
                    case ANS_STREAM_START: {
                        while(readMessage(socketConnection, msg) == 0 && msg->action != ANS_STREAM_END) {
                            ppf(CLR_HIGHLIGHT); printSave("WF CLIENT> Espulso file remoto '%s' (%d bytes) per fare spazio a %s (%ld bytes)", msg->path, msg->data_length, pathname, sb.st_size); ppff();
                            if(dirname == NULL) {
                                ppf(CLR_WARNING); printSave("WF CLIENT> Il file remoto '%s' (%d bytes) non verrà salvato perché non è stata specificata nessuna cartella (dirname = NULL).", msg->path, msg->data_length); ppff();
                            } else {
                                FILE* filePointer;
                                char percorso[PATH_MAX];
                                memset(percorso, 0, PATH_MAX);

                                strcpy(percorso, dirname);
                                
                                if(percorso[PATH_MAX - 1] != '/') {
                                    strcat(percorso, "/");
                                }
                                strcat(percorso, basename(msg->path));

                                filePointer = fopen(percorso, "wb");
                                if(filePointer == NULL) { 
                                    ppf(CLR_ERROR); printSave("WF CLIENT> Impossibile salvare file remoto nel percorso specificato: '%s'.", percorso);
                                    continue;
                                }

                                fwrite(msg->data, 1, msg->data_length, filePointer);
                                ppf(CLR_INFO); printSave("WF CLIENT> File remoto '%s' (%d bytes) salvato in '%s'", msg->path, msg->data_length, percorso); ppff();
                                
                                fclose(filePointer);
                                // free(msg->path); // FIXME
                                free(msg->data);
                            }
                        }
                        
                        if(readMessage(socketConnection, msg) == 0) {
                            if(msg->action == ANS_OK) {
                                printSave("WF CLIENT> File '%s' (%ld bytes) caricato!", pathname, sb.st_size);
                            } else {
                                ppf(CLR_ERROR); printSave("WF CLIENT> Il server ha mandato una risposta non valida (2). ACTION: %d", msg->action); ppff();
                                errno = EBADRQC;
                            }
                        }
                        break;
                    }

                    case ANS_BAD_RQST: {
                        ppf(CLR_ERROR); printSave("WF CLIENT> Il file '%s' (%ld bytes) non è stato creato e lockato di recente. Impossibile scriverci.", pathname, sb.st_size); ppff();
                        errno = EINVAL;
                        break;
                    }

                    case ANS_FILE_NOT_EXISTS: {
                        ppf(CLR_ERROR); printSave("WF CLIENT> Il file '%s' (%ld bytes) non esiste, devi fare prima una CREATE.", pathname, sb.st_size); ppff();
                        errno = ENOENT;
                        break;
                    }

                    default: {
                        ppf(CLR_ERROR); printSave("WF CLIENT> Il server ha mandato una risposta non valida. ACTION: %d", msg->action); ppff();
                        errno = EBADRQC;
                    }
                }
            } else { // il server non ha risposto al client
                printSave("WF CLIENT> Il server non ha risposto alla richiesta. ACTION: %d", msg->action);
                errno = EBADMSG;
            }
        } else { // impossibile inviare la richiesta
            printSave("WF CLIENT> Non è stato possibile inviare la richiesta. ACTION: %d", msg->action);
            errno = EBADE;
        }

        if(msg->action == ANS_OK) {
            free(msg);
            return 0;
        } else {
            free(msg);
            return -1;
        }
    } else { // non c'è un socket su cui comunicare
        errno = EPIPE;
        return -1;
    }
}


int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname) {
    // creo variabile per contenere richieste
    Message* msg = calloc(4, sizeof(Message));
    checkStop(msg == NULL, "malloc risposta");
    // fine variabile per contenere richieste

    setMessage(msg, REQ_APPEND, 0, (char*)pathname, buf, size);

    int socketConnection;

    if((socketConnection = searchAssocByName(socketPath)) != -1) {
        if(sendMessage(socketConnection, msg) == 0) {
            if(readMessage(socketConnection, msg) == 0) {
                switch(msg->action) {
                    case ANS_OK: {
                        printSave("AF CLIENT> Buffer da appendere (%zu bytes) caricato!", size);
                        break;
                    }
                    
                    case ANS_STREAM_START: {
                        while(readMessage(socketConnection, msg) == 0 && msg->action != ANS_STREAM_END) {
                            ppf(CLR_HIGHLIGHT); printSave("AF CLIENT> Espulso file remoto '%s' (%d bytes) per fare spazio al buffer da appendere (%zu bytes)", msg->path, msg->data_length, size); ppff();
                            if(dirname == NULL) {
                                ppf(CLR_WARNING); printSave("AF CLIENT> Il file remoto '%s' (%d bytes) non verrà salvato perché non è stata specificata nessuna cartella (dirname = NULL).", msg->path, msg->data_length); ppff();
                            } else {
                                FILE* filePointer;
                                char percorso[PATH_MAX];
                                memset(percorso, 0, strlen(percorso));

                                strcpy(percorso, dirname);
                                
                                if(percorso[strlen(percorso) - 1] != '/') {
                                    strcat(percorso, "/");
                                }
                                strcat(percorso, basename(msg->path));

                                filePointer = fopen(percorso, "wb");
                                if(filePointer == NULL) { 
                                    ppf(CLR_ERROR); printSave("AF CLIENT> Impossibile salvare file remoto nel percorso specificato: '%s'.", percorso);
                                    continue;
                                }

                                fwrite(msg->data, 1, msg->data_length, filePointer);
                                ppf(CLR_INFO); printSave("AF CLIENT> File remoto '%s' (%d bytes) salvato in '%s'", msg->path, msg->data_length, percorso); ppff();
                                
                                fclose(filePointer);
                                // free(msg->path); // FIXME
                                free(msg->data);
                            }
                        }
                        
                        if(readMessage(socketConnection, msg) == 0) {
                            if(msg->action == ANS_OK) {
                                printSave("AF CLIENT> Buffer da appendere (%zu bytes) caricato!", size);
                            } else {
                                ppf(CLR_ERROR); printSave("AF CLIENT> Il server ha mandato una risposta non valida (2). ACTION: %d", msg->action); ppff();
                                errno = EBADRQC;
                            }
                        }
                        break;
                    }

                    case ANS_NO_PERMISSION: {
                        ppf(CLR_ERROR); printSave("AF CLIENT> Il file '%s' non può essere unlockato perché non ne possiedi i diritti.", pathname); ppff();
                        errno = ENOENT;
                        break;
                    }

                    case ANS_FILE_NOT_EXISTS: {
                        ppf(CLR_ERROR); printSave("AF CLIENT> Il file '%s' (%zu bytes) non esiste, devi fare prima una CREATE.", pathname, size); ppff();
                        errno = ENOENT;
                        break;
                    }

                    default: {
                        ppf(CLR_ERROR); printSave("AF CLIENT> Il server ha mandato una risposta non valida. ACTION: %d", msg->action); ppff();
                        errno = EBADRQC;
                    }
                }
            } else { // il server non ha risposto al client
                printSave("AF CLIENT> Il server non ha risposto alla richiesta. ACTION: %d", msg->action);
                errno = EBADMSG;
            }
        } else { // impossibile inviare la richiesta
            printSave("AF CLIENT> Non è stato possibile inviare la richiesta. ACTION: %d", msg->action);
            errno = EBADE;
        }

        if(msg->action == ANS_OK) {
            free(msg);
            return 0;
        } else {
            free(msg);
            return -1;
        }
    } else { // non c'è un socket su cui comunicare
        errno = EPIPE;
        return -1;
    }
}


int lockFile(const char* pathname) {
    // creo variabile per contenere richieste
    Message* msg = calloc(4, sizeof(Message));
    checkStop(msg == NULL, "malloc risposta");
    // fine variabile per contenere richieste

    setMessage(msg, REQ_LOCK, 0, (char*)pathname, NULL, 0);

    int socketConnection;

    if((socketConnection = searchAssocByName(socketPath)) != -1) {
        if(sendMessage(socketConnection, msg) == 0) {
            if(readMessage(socketConnection, msg) == 0) {
                switch(msg->action) {
                    case ANS_OK: {
                        ppf(CLR_IMPORTANT); printSave("LF CLIENT> File '%s' lockato! (REQ_LOCK)", pathname); ppff();
                        break;
                    }

                    case ANS_FILE_NOT_EXISTS: {
                        ppf(CLR_ERROR); printSave("LF CLIENT> Il file '%s' non esiste (o è stato cancellato mentre eri in attesa), non puoi lockarlo.", pathname); ppff();
                        errno = ENOENT;
                        break;
                    }

                    default: {
                        ppf(CLR_ERROR); printSave("LF CLIENT> Il server ha mandato una risposta non valida. ACTION: %d", msg->action); ppff();
                        errno = EBADRQC;
                    }
                }
            } else { // il server non ha risposto al client
                printSave("LF CLIENT> Il server non ha risposto alla richiesta. ACTION: %d", msg->action);
                errno = EBADMSG;
            }
        } else { // impossibile inviare la richiesta
            printSave("LF CLIENT> Non è stato possibile inviare la richiesta. ACTION: %d", msg->action);
            errno = EBADE;
        }

        if(msg->action == ANS_OK) {
            free(msg);
            return 0;
        } else {
            free(msg);
            return -1;
        }
    } else { // non c'è un socket su cui comunicare
        errno = EPIPE;
        return -1;
    }
}


int unlockFile(const char* pathname) {
    // creo variabile per contenere richieste
    Message* msg = calloc(4, sizeof(Message));
    checkStop(msg == NULL, "malloc risposta");
    // fine variabile per contenere richieste

    setMessage(msg, REQ_UNLOCK, 0, (char*)pathname, NULL, 0);

    int socketConnection;

    if((socketConnection = searchAssocByName(socketPath)) != -1) {
        if(sendMessage(socketConnection, msg) == 0) {
            if(readMessage(socketConnection, msg) == 0) {
                switch(msg->action) {
                    case ANS_OK: {
                        ppf(CLR_IMPORTANT); printSave("UF CLIENT> File '%s' unlockato! (REQ_UNLOCK)", pathname); ppff();
                        break;
                    }

                    case ANS_NO_PERMISSION: {
                        ppf(CLR_ERROR); printSave("UF CLIENT> Il file '%s' non può essere unlockato perché non ne possiedi i diritti.", pathname); ppff();
                        errno = ENOENT;
                        break;
                    }

                    case ANS_FILE_NOT_EXISTS: {
                        ppf(CLR_ERROR); printSave("UF CLIENT> Il file '%s' non esiste, non puoi lockarlo.", pathname); ppff();
                        errno = ENOENT;
                        break;
                    }

                    default: {
                        ppf(CLR_ERROR); printSave("UF CLIENT> Il server ha mandato una risposta non valida. ACTION: %d", msg->action); ppff();
                        errno = EBADRQC;
                    }
                }
            } else { // il server non ha risposto al client
                printSave("UF CLIENT> Il server non ha risposto alla richiesta. ACTION: %d", msg->action);
                errno = EBADMSG;
            }
        } else { // impossibile inviare la richiesta
            printSave("UF CLIENT> Non è stato possibile inviare la richiesta. ACTION: %d", msg->action);
            errno = EBADE;
        }

        if(msg->action == ANS_OK) {
            free(msg);
            return 0;
        } else {
            free(msg);
            return -1;
        }
    } else { // non c'è un socket su cui comunicare
        errno = EPIPE;
        return -1;
    }
}


int closeFile(const char* pathname) {
    // creo variabile per contenere richieste
    Message* msg = calloc(4, sizeof(Message));
    checkStop(msg == NULL, "malloc risposta");
    // fine variabile per contenere richieste

    setMessage(msg, REQ_CLOSE, 0, (char*)pathname, NULL, 0); // inizializzo

    int socketConnection;

    if((socketConnection = searchAssocByName(socketPath)) != -1) {
        if(sendMessage(socketConnection, msg) == 0) {
            int esito = readMessage(socketConnection, msg);
            if(esito == 0) {
                switch(msg->action) {
                    case ANS_OK: {
                        ppf(CLR_HIGHLIGHT); printSave("CF CLIENT> File '%s' chiuso con successo.", pathname); ppff();
                        break;
                    }

                    case ANS_NO_PERMISSION: {
                        ppf(CLR_ERROR); printSave("CF CLIENT> Il file '%s' non può essere chiuso perché è lockato da un altro client.", pathname); ppff();
                        errno = EACCES;
                        break;
                    }

                    case ANS_FILE_NOT_EXISTS: {
                        ppf(CLR_ERROR); printSave("CF CLIENT> Il file '%s' non esiste, non puoi chiuderlo.", pathname); ppff();
                        errno = ENOENT;
                        break;
                    }

                    default: {
                        ppf(CLR_ERROR); printSave("CF CLIENT> Il server ha mandato una risposta non valida. ACTION: %d", msg->action); ppff();
                        errno = EBADRQC;
                    }
                }
            } else { // il server non ha risposto al client
                printSave("CF CLIENT> Il server non ha risposto alla richiesta. ACTION: %d", msg->action);
                errno = EBADMSG;
            }
        } else { // impossibile inviare la richiesta
            printSave("CF CLIENT> Non è stato possibile inviare la richiesta. ACTION: %d", msg->action);
            errno = EBADE;
        }

        if(msg->action == ANS_OK) {
            free(msg);
            return 0;
        } else {
            free(msg);
            return -1;
        }
    } else { // non c'è un socket su cui comunicare
        errno = EPIPE;
        return -1;
    }

    return 0;
}


int removeFile(const char* pathname) {
    // creo variabile per contenere richieste
    Message* msg = calloc(4, sizeof(Message));
    checkStop(msg == NULL, "malloc risposta");
    // fine variabile per contenere richieste

    setMessage(msg, REQ_DELETE, 0, (char*)pathname, NULL, 0); // inizializzo

    int socketConnection;

    if((socketConnection = searchAssocByName(socketPath)) != -1) {
        if(sendMessage(socketConnection, msg) == 0) {
            int esito = readMessage(socketConnection, msg);
            if(esito == 0) {
                switch(msg->action) {
                    case ANS_OK: {
                        ppf(CLR_HIGHLIGHT); printSave("RF CLIENT> File '%s' eliminato con successo.", pathname); ppff();
                        break;
                    }

                    case ANS_NO_PERMISSION: {
                        ppf(CLR_ERROR); printSave("RF CLIENT> Il file '%s' non può essere eliminato perché è lockato da un altro client.", pathname); ppff();
                        errno = EACCES;
                        break;
                    }

                    case ANS_FILE_NOT_EXISTS: {
                        ppf(CLR_ERROR); printSave("RF CLIENT> Il file '%s' non esiste, non puoi eliminarlo.", pathname); ppff();
                        errno = ENOENT;
                        break;
                    }

                    default: {
                        ppf(CLR_ERROR); printSave("RF CLIENT> Il server ha mandato una risposta non valida. ACTION: %d", msg->action); ppff();
                        errno = EBADRQC;
                    }
                }
            } else { // il server non ha risposto al client
                printSave("RF CLIENT> Il server non ha risposto alla richiesta. ACTION: %d", msg->action);
                errno = EBADMSG;
            }
        } else { // impossibile inviare la richiesta
            printSave("RF CLIENT> Non è stato possibile inviare la richiesta. ACTION: %d", msg->action);
            errno = EBADE;
        }

        if(msg->action == ANS_OK) {
            free(msg);
            return 0;
        } else {
            free(msg);
            return -1;
        }
    } else { // non c'è un socket su cui comunicare
        errno = EPIPE;
        return -1;
    }

    return 0;
}
