/**
 * @file    api.c
 * @brief   Contiene l'implementazione delle funzioni dell'api che permette al client di collegarsi al server.
 * @author  Leonardo Pantani
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
#include <linux/limits.h>
#include <time.h>

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
    
    // creo variabile per contenere richieste
    Message* msg = calloc(4, sizeof(Message));
    checkStop(msg == NULL, "malloc risposta");
    // fine variabile per contenere richieste

    setMessage(msg, REQ_OPEN, flags, (char*)pathname, NULL, 0); // inizializzo

    int socketConnection;
    extern char ejectedFileFolder[PATH_MAX];

    if((socketConnection = searchAssocByName(socketPath)) != -1) {
        setMessage(msg, REQ_OPEN, flags, (char*)pathname, NULL, 0);

        if(sendMessage(socketConnection, msg) == 0) {
            // una volta inviata la richiesta di check del file
            int esito = readMessage(socketConnection, msg);
            if(esito == 0) {
                switch(msg->action) {
                    case ANS_OK: {
                        printf("CLIENT> File '%s' aperto!\n", pathname); fflush(stdout);
                        if(flags == 2 || flags == 3) {
                            ppf(CLR_IMPORTANT); printf("CLIENT> File '%s' lockato!\n", pathname); ppff();
                        }
                        break;
                    }

                    case ANS_BAD_RQST: {
                        printf("CLIENT> Richiesta errata!\n"); fflush(stdout);
                        errno = EBADRQC;
                        return -1;
                    }

                    case ANS_STREAM_START: {
                        esito = readMessage(socketConnection, msg);
                        while(esito == 0 && msg->action != ANS_STREAM_END) {
                            ppf(CLR_HIGHLIGHT); printf("CLIENT> Espulso file remoto '%s' (%d bytes) per fare spazio a %s\n", msg->path, msg->data_length, pathname); ppff();
                            if(strcmp(ejectedFileFolder, "#") == 0) {
                                ppf(CLR_WARNING); printf("CLIENT> Il file remoto '%s' (%d bytes) non verrà salvato perché non è stata specificata nessuna cartella con -D.\n", msg->path, msg->data_length); ppff();
                            } else {
                                FILE* filePointer;
                                char percorso[PATH_MAX];
                                memset(percorso, 0, sizeof(percorso));

                                strcpy(percorso, ejectedFileFolder);
                                
                                if(percorso[strlen(percorso) - 1] != '/') {
                                    strcat(percorso, "/");
                                }
                                strcat(percorso, basename(msg->path));

                                filePointer = fopen(percorso, "wb");
                                if(filePointer == NULL) { 
                                    ppf(CLR_ERROR); printf("CLIENT> Impossibile salvare file remoto nel percorso specificato: '%s'!\n", percorso);
                                    continue;
                                }

                                fwrite(msg->data, 1, msg->data_length, filePointer);
                                ppf(CLR_INFO); printf("CLIENT> File remoto '%s' (%d bytes) salvato in '%s'\n", msg->path, msg->data_length, percorso); ppff();
                                
                                fclose(filePointer);
                            }
                            esito = readMessage(socketConnection, msg);
                        }
                        
                        esito = readMessage(socketConnection, msg);
                        if(esito == 0) {
                            if(msg->action == ANS_OK) {
                                printf("CLIENT> File '%s' aperto!\n", pathname); fflush(stdout);
                                if(flags == 2 || flags == 3) {
                                    ppf(CLR_IMPORTANT); printf("CLIENT> File '%s' lockato!\n", pathname); ppff();
                                }
                            }
                        }
                            
                        break;
                    }

                    case ANS_FILE_EXISTS: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' esiste già, non puoi fare una CREATE.\n", pathname); ppff();
                        errno = EEXIST;
                        return -1;
                    }

                    case ANS_FILE_NOT_EXISTS: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' non esiste, devi fare prima una CREATE.\n", pathname); ppff();
                        errno = ENOENT;
                        return -1;
                    }

                    default: {
                        ppf(CLR_ERROR); printf("CLIENT> Il server ha mandato una risposta non valida  %d\n", msg->action); ppff();
                        errno = EBADRQC;
                        return -1;
                    }
                }
            } else { // errore risposta
                stampaDebug("Errore body richiesta visione file!");
                errno = EBADMSG;
                return -1;
            }
        } else { // errore invio msg
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


int readFile(const char* pathname, void** buf, size_t* size) {
    // creo variabile per contenere richieste
    Message* msg = calloc(4, sizeof(Message));
    checkStop(msg == NULL, "malloc risposta");
    // fine variabile per contenere richieste

    setMessage(msg, REQ_READ, 0, (char*)pathname, NULL, 0); // inizializzo

    int socketConnection;

    if((socketConnection = searchAssocByName(socketPath)) != -1) {
        if(sendMessage(socketConnection, msg) == 0) {
            // una volta inviata la richiesta di check del file
            int esito = readMessage(socketConnection, msg);
            if(esito == 0) {
                switch(msg->action) {
                    case ANS_OK: {
                        *buf = msg->data;
                        *size = msg->data_length;
                        ppf(CLR_INFO); printf("CLIENT> File '%s' salvato in memoria.\n", pathname); ppff();
                        break;
                    }

                    case ANS_NO_PERMISSION: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' non può essere letto perché è lockato da un altro client.\n", pathname); ppff();
                        errno = EACCES;
                        return -1;
                    }

                    case ANS_FILE_NOT_EXISTS: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' non esiste, non può essere letto.\n", pathname); ppff();
                        errno = ENOENT;
                        return -1;
                    }

                    default: {
                        ppf(CLR_ERROR); printf("CLIENT> Il server ha mandato una risposta non valida: %s", msg->data); ppff();
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


int readNFiles(int N, const char* dirname) {
    // creo variabile per contenere richieste
    Message* msg = calloc(4, sizeof(Message));
    checkStop(msg == NULL, "malloc risposta");
    // fine variabile per contenere richieste

    if(N <= 0) { // se N <= 0 allora vanno letti tutti i file, lo imposto a -1 così il server capisce cosa intendo
        N = -1;
    }

    setMessage(msg, REQ_READ_N, 0, NULL, &N, sizeof(int)); // inizializzo

    int socketConnection;
    extern char savedFileFolder[PATH_MAX];

    if((socketConnection = searchAssocByName(socketPath)) != -1) {
        if(sendMessage(socketConnection, msg) == 0) {
            // una volta inviata la richiesta di check del file
            int esito = readMessage(socketConnection, msg);
            if(esito == 0) {
                switch(msg->action) {
                    case ANS_STREAM_START: {
                        esito = readMessage(socketConnection, msg);
                        while(esito == 0 && msg->action != ANS_STREAM_END) {
                            ppf(CLR_HIGHLIGHT); printf("CLIENT> File remoto '%s' ricevuto dal server.\n", msg->path); ppff();
                            if(strcmp(savedFileFolder, "#") == 0) {
                                ppf(CLR_WARNING); printf("CLIENT> Il file remoto '%s' (%d bytes) non verrà salvato perché non è stata specificata nessuna cartella con -d.\n", msg->path, msg->data_length); ppff();
                            } else {
                                FILE* filePointer;
                                char percorso[PATH_MAX];
                                memset(percorso, 0, strlen(percorso));

                                strcpy(percorso, savedFileFolder);
                                
                                if(percorso[strlen(percorso) - 1] != '/') {
                                    strcat(percorso, "/");
                                }
                                strcat(percorso, basename(msg->path));

                                filePointer = fopen(percorso, "wb");
                                if(filePointer == NULL) { 
                                    ppf(CLR_ERROR); printf("CLIENT> Impossibile salvare file remoto nel percorso specificato: '%s'!\n", percorso);
                                    continue;
                                }

                                fwrite(msg->data, 1, msg->data_length, filePointer);
                                ppf(CLR_INFO); printf("CLIENT> File remoto '%s' (%d bytes) salvato in '%s'\n", msg->path, msg->data_length, percorso); ppff();
                                
                                fclose(filePointer);
                            }
                            esito = readMessage(socketConnection, msg);
                        }
                            
                        break;
                    }

                    case ANS_FILE_NOT_EXISTS: { // modo simpatico per dire che non ci sono file da mandare
                        ppf(CLR_ERROR); printf("CLIENT> Il server non possiede file da inviarti.\n"); ppff();
                        errno = ENOENT;
                        return -1;
                    }

                    default: {
                        ppf(CLR_ERROR); printf("CLIENT> Il server ha mandato una risposta non valida: %s", msg->data); ppff();
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
            return -1;
        }

        struct stat sb;
        if(stat(pathname, &sb) == -1) { // impossibile aprire statistiche file
            errno = EBADFD;
            return -1;
        }

        void *buffer = malloc(sb.st_size);
        fread(buffer, sb.st_size, 1, file);

        fclose(file);

        setMessage(msg, REQ_WRITE, 0, (char*)pathname, buffer, sb.st_size);

        if(sendMessage(socketConnection, msg) == 0) {
            free(buffer);
            // una volta inviata la richiesta di check del file
            int esito = readMessage(socketConnection, msg);
            if(esito == 0) {
                switch(msg->action) {
                    case ANS_OK: {
                        printf("CLIENT> File '%s' (%ld bytes) caricato!\n", pathname, sb.st_size); fflush(stdout);
                        break;
                    }
                    
                    case ANS_STREAM_START: {
                        esito = readMessage(socketConnection, msg);
                        while(esito == 0 && msg->action != ANS_STREAM_END) {
                            ppf(CLR_HIGHLIGHT); printf("CLIENT> Espulso file remoto '%s' (%d bytes) per fare spazio a %s (%ld bytes)\n", msg->path, msg->data_length, pathname, sb.st_size); ppff();
                            if(dirname == NULL) {
                                ppf(CLR_WARNING); printf("CLIENT> Il file remoto '%s' (%d bytes) non verrà salvato perché non è stata specificata nessuna cartella (dirname = NULL).\n", msg->path, msg->data_length); ppff();
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
                                    ppf(CLR_ERROR); printf("CLIENT> Impossibile salvare file remoto nel percorso specificato: '%s'!\n", percorso);
                                    continue;
                                }

                                fwrite(msg->data, 1, msg->data_length, filePointer);
                                ppf(CLR_INFO); printf("CLIENT> File remoto '%s' (%d bytes) salvato in '%s'\n", msg->path, msg->data_length, percorso); ppff();
                                
                                fclose(filePointer);
                            }
                            esito = readMessage(socketConnection, msg);
                        }
                        
                        esito = readMessage(socketConnection, msg);
                        if(esito == 0) {
                            if(msg->action == ANS_OK) {
                                printf("CLIENT> File '%s' (%ld bytes) caricato!\n", pathname, sb.st_size); fflush(stdout);
                            }
                        }
                            
                        break;
                    }

                    case ANS_BAD_RQST: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' (%ld bytes) non è stato creato e lockato di recente. Impossibile scriverci.\n", pathname, sb.st_size); ppff();
                        errno = EINVAL;
                        return -1;
                    }

                    case ANS_FILE_NOT_EXISTS: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' (%ld bytes) non esiste, devi fare prima una CREATE.\n", pathname, sb.st_size); ppff();
                        errno = ENOENT;
                        return -1;
                    }

                    default: {
                        ppf(CLR_ERROR); printf("CLIENT> Il server ha mandato una risposta non valida: %s (azione codice %d)\n", msg->data, msg->action); ppff();
                        errno = EBADRQC;
                        return -1;
                    }
                }
            } else { // errore risposta
                stampaDebug("Errore body richiesta visione file!");
                errno = EBADMSG;
                return -1;
            }
        } else { // errore invio msg
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


int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname) {
    // creo variabile per contenere richieste
    Message* msg = calloc(4, sizeof(Message));
    checkStop(msg == NULL, "malloc risposta");
    // fine variabile per contenere richieste

    setMessage(msg, ANS_UNKNOWN, 0, NULL, NULL, 0); // inizializzo

    int socketConnection;

    if((socketConnection = searchAssocByName(socketPath)) != -1) {
        setMessage(msg, REQ_APPEND, 0, (char*)pathname, buf, size);

        if(sendMessage(socketConnection, msg) == 0) {
            // una volta inviata la richiesta di check del file
            int esito = readMessage(socketConnection, msg);
            if(esito == 0) {
                switch(msg->action) {
                    case ANS_OK: {
                        printf("CLIENT> Buffer da appendere (%zu bytes) caricato!\n", size); fflush(stdout);
                        break;
                    }
                    
                    case ANS_STREAM_START: {
                        esito = readMessage(socketConnection, msg);
                        while(esito == 0 && msg->action != ANS_STREAM_END) {
                            ppf(CLR_HIGHLIGHT); printf("CLIENT> Espulso file remoto '%s' (%d bytes) per fare spazio al buffer da appendere (%zu bytes)\n", msg->path, msg->data_length, size); ppff();
                            if(dirname == NULL) {
                                ppf(CLR_WARNING); printf("CLIENT> Il file remoto '%s' (%d bytes) non verrà salvato perché non è stata specificata nessuna cartella (dirname = NULL).\n", msg->path, msg->data_length); ppff();
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
                                    ppf(CLR_ERROR); printf("CLIENT> Impossibile salvare file remoto nel percorso specificato: '%s'!\n", percorso);
                                    continue;
                                }

                                fwrite(msg->data, 1, msg->data_length, filePointer);
                                ppf(CLR_INFO); printf("CLIENT> File remoto '%s' (%d bytes) salvato in '%s'\n", msg->path, msg->data_length, percorso); ppff();
                                
                                fclose(filePointer);
                            }
                            esito = readMessage(socketConnection, msg);
                        }
                        
                        esito = readMessage(socketConnection, msg);
                        if(esito == 0) {
                            if(msg->action == ANS_OK) {
                                printf("CLIENT> Buffer da appendere (%zu bytes) caricato!\n", size); fflush(stdout);
                            }
                        }
                            
                        break;
                    }

                    case ANS_NO_PERMISSION: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' non può essere unlockato perché non ne possiedi i diritti.\n", pathname); ppff();
                        errno = ENOENT;
                        return -1;
                    }

                    case ANS_FILE_NOT_EXISTS: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' (%zu bytes) non esiste, devi fare prima una CREATE.\n", pathname, size); ppff();
                        errno = ENOENT;
                        return -1;
                    }

                    default: {
                        ppf(CLR_ERROR); printf("CLIENT> Il server ha mandato una risposta non valida: %s (azione codice %d)\n", msg->data, msg->action); ppff();
                        errno = EBADRQC;
                        return -1;
                    }
                }
            } else { // errore risposta
                stampaDebug("Errore body richiesta visione file!");
                errno = EBADMSG;
                return -1;
            }
        } else { // errore invio msg
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


int lockFile(const char* pathname) {
    // creo variabile per contenere richieste
    Message* msg = calloc(4, sizeof(Message));
    checkStop(msg == NULL, "malloc risposta");
    // fine variabile per contenere richieste

    setMessage(msg, ANS_UNKNOWN, 0, NULL, NULL, 0); // inizializzo

    int socketConnection;

    if((socketConnection = searchAssocByName(socketPath)) != -1) {
        setMessage(msg, REQ_LOCK, 0, (char*)pathname, NULL, 0);

        if(sendMessage(socketConnection, msg) == 0) {
            int esito = readMessage(socketConnection, msg);
            if(esito == 0) {
                switch(msg->action) {
                    case ANS_OK: {
                        ppf(CLR_IMPORTANT); printf("CLIENT> File '%s' lockato! (REQ_LOCK)\n", pathname); ppff();
                        break;
                    }

                    case ANS_FILE_NOT_EXISTS: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' non esiste (o è stato cancellato mentre eri in attesa), non puoi lockarlo.\n", pathname); ppff();
                        errno = ENOENT;
                        return -1;
                    }

                    default: {
                        ppf(CLR_ERROR); printf("CLIENT> Il server ha mandato una risposta non valida: %s (azione codice %d)\n", msg->data, msg->action); ppff();
                        errno = EBADRQC;
                        return -1;
                    }
                }
            } else { // errore risposta
                stampaDebug("Errore body richiesta visione file!");
                errno = EBADMSG;
                return -1;
            }
        } else { // errore invio msg
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


int unlockFile(const char* pathname) {
    // creo variabile per contenere richieste
    Message* msg = calloc(4, sizeof(Message));
    checkStop(msg == NULL, "malloc risposta");
    // fine variabile per contenere richieste

    setMessage(msg, ANS_UNKNOWN, 0, NULL, NULL, 0); // inizializzo

    int socketConnection;

    if((socketConnection = searchAssocByName(socketPath)) != -1) {
        setMessage(msg, REQ_UNLOCK, 0, (char*)pathname, NULL, 0);

        if(sendMessage(socketConnection, msg) == 0) {
            int esito = readMessage(socketConnection, msg);
            if(esito == 0) {
                switch(msg->action) {
                    case ANS_OK: {
                        ppf(CLR_IMPORTANT); printf("CLIENT> File '%s' unlockato! (REQ_UNLOCK)\n", pathname); ppff();
                        break;
                    }

                    case ANS_NO_PERMISSION: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' non può essere unlockato perché non ne possiedi i diritti.\n", pathname); ppff();
                        errno = ENOENT;
                        return -1;
                    }

                    case ANS_FILE_NOT_EXISTS: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' non esiste, non puoi lockarlo.\n", pathname); ppff();
                        errno = ENOENT;
                        return -1;
                    }

                    default: {
                        ppf(CLR_ERROR); printf("CLIENT> Il server ha mandato una risposta non valida: %s (azione codice %d)\n", msg->data, msg->action); ppff();
                        errno = EBADRQC;
                        return -1;
                    }
                }
            } else { // errore risposta
                stampaDebug("Errore body richiesta visione file!");
                errno = EBADMSG;
                return -1;
            }
        } else { // errore invio msg
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


int closeFile(const char* pathname) {
    // creo variabile per contenere richieste
    Message* msg = calloc(4, sizeof(Message));
    checkStop(msg == NULL, "malloc risposta");
    // fine variabile per contenere richieste

    setMessage(msg, REQ_CLOSE, 0, (char*)pathname, NULL, 0); // inizializzo

    int socketConnection;

    if((socketConnection = searchAssocByName(socketPath)) != -1) {
        if(sendMessage(socketConnection, msg) == 0) {
            // una volta inviata la richiesta di check del file
            int esito = readMessage(socketConnection, msg);
            if(esito == 0) {
                switch(msg->action) {
                    case ANS_OK: {
                        ppf(CLR_HIGHLIGHT); printf("CLIENT> File '%s' chiuso con successo.\n", pathname); ppff();
                        break;
                    }

                    case ANS_NO_PERMISSION: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' non può essere chiuso perché è lockato da un altro client.\n", pathname); ppff();
                        errno = EACCES;
                        return -1;
                    }

                    case ANS_FILE_NOT_EXISTS: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' non esiste, non puoi chiuderlo.\n", pathname); ppff();
                        errno = ENOENT;
                        return -1;
                    }

                    default: {
                        ppf(CLR_ERROR); printf("CLIENT> Il server ha mandato una risposta non valida: %s", msg->data); ppff();
                        errno = EBADRQC;
                        return -1;
                    }
                }
            } else { // errore risposta
                stampaDebug("Errore body richiesta chiusura file!");
                errno = EBADMSG;
                return -1;
            }
        }
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
            // una volta inviata la richiesta di check del file
            int esito = readMessage(socketConnection, msg);
            if(esito == 0) {
                switch(msg->action) {
                    case ANS_OK: {
                        ppf(CLR_HIGHLIGHT); printf("CLIENT> File '%s' eliminato con successo.\n", pathname); ppff();
                        break;
                    }

                    case ANS_NO_PERMISSION: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' non può essere eliminato perché è lockato da un altro client.\n", pathname); ppff();
                        errno = EACCES;
                        return -1;
                    }

                    case ANS_FILE_NOT_EXISTS: {
                        ppf(CLR_ERROR); printf("CLIENT> Il file '%s' non esiste, non puoi eliminarlo.\n", pathname); ppff();
                        errno = ENOENT;
                        return -1;
                    }

                    default: {
                        ppf(CLR_ERROR); printf("CLIENT> Il server ha mandato una risposta non valida: %s", msg->data); ppff();
                        errno = EBADRQC;
                        return -1;
                    }
                }
            } else { // errore risposta
                stampaDebug("Errore body richiesta eliminazione file!");
                errno = EBADMSG;
                return -1;
            }
        }
    }

    return 0;
}
