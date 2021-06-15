/**
 * @file    client.c
 * @brief   Contiene l'implementazione del client, tra cui la lettura dei parametri in ingresso ed esecuzione delle richieste verso il server.
 * @author  Leonardo Pantani
**/

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <getopt.h>

#include "api.h"

FILE* fileLog;
pthread_mutex_t mutexFileLog = PTHREAD_MUTEX_INITIALIZER;

int verbose = FALSE; // stampa per ogni operazione

long timeoutRequests = 0; // tempo tra una richiesta e l'altra

char ejectedFileFolder[PATH_MAX]; // -D percorso dove salvare file espulsi dal server (serve w|W)

char savedFileFolder[PATH_MAX]; // -d percorso dove salvare i file letti dal server (serve r|R)

int chiusuraForte = FALSE;
pthread_mutex_t mutexChiusura = PTHREAD_MUTEX_INITIALIZER;

void help(void) {
    ppf(CLR_HIGHLIGHT); printf("Utilizzo:\n"); ppff();
    ppf(CLR_INFO); printf("    -h                 \033[0m->\033[94m stampa questa lista\n"); ppff();
    ppf(CLR_INFO); printf("    -f <filename>      \033[0m->\033[94m specifica il nome del socket a cui connettersi\n"); ppff();
    ppf(CLR_INFO); printf("    -w <dirname[,n=0]> \033[0m->\033[94m invia i file nella cartella dirname; se n=0 manda tutti i file, altrimenti manda n file\n"); ppff();
    ppf(CLR_INFO); printf("    -W <file1[,file2]> \033[0m->\033[94m lista di file da inviare al server\n"); ppff();
    ppf(CLR_INFO); printf("    -D <dirname>       \033[0m->\033[94m cartella dove mettere i file in caso vengano espulsi dal server\n"); ppff();
    ppf(CLR_INFO); printf("    -r <file1[,file2]> \033[0m->\033[94m lista di file richiesti al server\n"); ppff();
    ppf(CLR_INFO); printf("    -R [n=0]           \033[0m->\033[94m legge n file dal server, se n non specificato li legge tutti\n"); ppff();
    ppf(CLR_INFO); printf("    -d <dirname>       \033[0m->\033[94m cartella dove mettere i file letti dal server\n"); ppff();
    ppf(CLR_INFO); printf("    -t time            \033[0m->\033[94m tempo (ms) che intercorre tra una richiesta di connessione e l'altra al server\n"); ppff();
    ppf(CLR_INFO); printf("    -l <file1[,file2]> \033[0m->\033[94m lista di file da bloccare\n"); ppff();
    ppf(CLR_INFO); printf("    -u <file1[,file2]> \033[0m->\033[94m lista di file da sbloccare\n"); ppff();
    ppf(CLR_INFO); printf("    -c <file1[,file2]> \033[0m->\033[94m lista di file da rimuovere dal server\n"); ppff();
    ppf(CLR_INFO); printf("    -p                 \033[0m->\033[94m abilita le stampe sull'stdout di ogni operazione\n"); ppff();
}

static int sendFilesList(char* nomeCartella, int numFiles, int completed, int total) {
    if(numFiles > 0 || numFiles == -1) {
        DIR* cartella;
        struct dirent *entry;

        if(!(cartella = opendir(nomeCartella))) return -1;

        while((entry = readdir(cartella)) != NULL && (numFiles > 0 || numFiles == -1)) {
            char path[PATH_MAX];
            snprintf(path, sizeof(path), "%s/%s", nomeCartella, entry->d_name);
            struct stat proprieta;

            if(stat(path, &proprieta) == 0) {
                if(S_ISDIR(proprieta.st_mode)) { // fix (?)
                    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                        continue;
                    sendFilesList(path, numFiles, completed, total);
                } else { // è un file
                    if(openFile(path, O_CREATE|O_LOCK) == 0) { // prima vede se il file va creato o c'è già
                        if(writeFile(path, ejectedFileFolder) == 0) { // se il file non esiste allora lo blocco e ci scrivo sopra
                            completed++;
                        }
                        //unlockFile(path);
                    } else if(openFile(path, O_LOCK) == 0) { // se il file esiste già allora faccio la lock e appendo
                        FILE* filePointer;
                        filePointer = fopen(path, "rb");
                        if(filePointer == NULL) { pe("File non aperto!"); }

                        void* data = malloc(sizeof(char)*proprieta.st_size);

                        if(fread(data, 1, proprieta.st_size, filePointer) == proprieta.st_size) { // se la lettura da file avviene correttamente
                            if(appendToFile(path, data, proprieta.st_size, ejectedFileFolder) == 0) { // allora faccio la append
                                completed++;
                            }
                            //unlockFile(path);
                        }

                        fclose(filePointer);
                        free(data);
                    } else { // c'è un problema relativo al file o qualcos'altro
                        // NULLA
                    }
                    total++;
                    
                    if(numFiles != -1) numFiles--;
                }
            }
        }
        closedir(cartella);
    }

    return total-completed;
}


static int executeAction(ActionType ac, char* parameters) {
    switch(ac) {
        case AC_WRITE_RECU: { // -w
            char* savePointer;
            char* nomeCartella, *temp;
            DIR* cartella;

            int numFiles = -1;
            
            nomeCartella = strtok_r(parameters, ",", &savePointer);
            if((cartella = opendir(nomeCartella)) == NULL) {
                ppf(CLR_ERROR); printSave("CLIENT> Cartella specificata '%s' inesistente.", nomeCartella); ppff();
                free(cartella);
                return -1;
            } else {
                ppf(CLR_HIGHLIGHT); printf("CLIENT> Cartella per la write impostata su '%s'", nomeCartella); fflush(stdout);
                free(cartella);
            }
            temp = strtok_r(NULL, ",", &savePointer);

            if(temp != NULL) {
                strtok_r(temp, "=", &savePointer);
                if((temp = strtok_r(NULL, ",", &savePointer)) != NULL) {
                    numFiles = atoi(temp);
                    
                    if(numFiles == 0) {
                        numFiles = -1;
                        printf(" | numero file: TUTTI\n"); ppff();
                    } else {
                        printf(" | numero file: %d\n", numFiles); ppff();
                    }
                    fflush(stdout);
                } else {
                    ppf(CLR_ERROR); printSave("CLIENT> Specificato sottoargomento 'n' ma non il suo valore."); ppff();
                    return -1;
                }
            } else {
                printf(" | numero file: TUTTI\n"); ppff(); fflush(stdout);
            }

            return sendFilesList(nomeCartella, numFiles, 0, 0); // restituisce la differenza tra file totali e file inviati con successo (se 0 ok, !=0 non tutti inviati)
        }

        case AC_WRITE_LIST: { // -W
            char* savePointer;
            char* nomeFile;

            int completed = 0;
            int total = 0;

            nomeFile = strtok_r(parameters, ",", &savePointer);
            while(nomeFile != NULL) {
                total++;

                struct stat proprieta;
                int esito = stat(nomeFile, &proprieta);

                if(esito == 0) {
                    if(S_ISDIR(proprieta.st_mode) == 0) {
                        if(openFile(nomeFile, O_CREATE|O_LOCK) == 0) { // prima vede se il file va creato o c'è già
                            if(writeFile(nomeFile, ejectedFileFolder) == 0) { // se il file non esiste allora lo blocco e ci scrivo sopra
                                completed++;
                            }
                            //unlockFile(nomeFile);
                        } else if(openFile(nomeFile, O_LOCK) == 0) { // se il file esiste già allora faccio la lock e appendo
                            FILE* filePointer;
                            filePointer = fopen(nomeFile, "rb");
                            if(filePointer == NULL) { pe("File non aperto!!"); }

                            void* data = malloc(sizeof(char)*proprieta.st_size);

                            if(fread(data, 1, proprieta.st_size, filePointer) == proprieta.st_size) { // se la lettura da file avviene correttamente
                                if(appendToFile(nomeFile, data, proprieta.st_size, ejectedFileFolder) == 0) { // allora faccio la append
                                    completed++;
                                }
                                //unlockFile(nomeFile);
                            }

                            fclose(filePointer);
                            free(data);
                        } else { // c'è un problema relativo al file o qualcos'altro
                            // NULLA
                        }
                    } else {
                        ppf(CLR_ERROR); printSave("CLIENT> '%s' è una cartella.", nomeFile); ppff();
                    }
                } else {
                    ppf(CLR_ERROR); printSave("CLIENT> Il file '%s' non esiste o è corrotto.", nomeFile); ppff();
                }

                nomeFile = strtok_r(NULL, ",", &savePointer);
            }

            if(total - completed != 0) { // almeno un file non è stato inviato
                return -1;
            }

            break;
        }

        case AC_READ_LIST: { // -r
            char* savePointer;
            char* nomeFile;

            void* puntatoreFile;
            size_t dimensioneFile;

            int saveFiles = TRUE;

            if(strcmp(savedFileFolder, "#") == 0) {
                ppf(CLR_WARNING); printSave("CLIENT> I file non saranno salvati perché non hai specificato nessuna cartella di destinazione con -d."); ppff();
                saveFiles = FALSE;
            }
            
            nomeFile = strtok_r(parameters, ",", &savePointer);
            while(nomeFile != NULL) {
                if(readFile(nomeFile, &puntatoreFile, &dimensioneFile) == 0) {
                    if(saveFiles) { // effettuo operazioni sottostanti solo se i file vanno salvati
                        FILE* filePointer;
                        char percorso[PATH_MAX];

                        memset(percorso, 0, PATH_MAX);

                        strcpy(percorso, savedFileFolder);
                                        
                        if(percorso[PATH_MAX - 1] != '/') {
                            strcat(percorso, "/");
                        }
                        strcat(percorso, basename(nomeFile));

                        filePointer = fopen(percorso, "wb");
                        if(filePointer == NULL) { pe("File non aperto!!"); }

                        fwrite(puntatoreFile, 1, dimensioneFile, filePointer);
                        ppf(CLR_INFO); printSave("CLIENT> File '%s' salvato in '%s'", nomeFile, percorso); ppff();

                        free(puntatoreFile);   
                        fclose(filePointer);
                    }
                } else {
                    return -1; // appena un file non viene letto do operazione fallita
                }
                
                nomeFile = strtok_r(NULL, ",", &savePointer);
            }
        }

        case AC_READ_RECU: { // -R
            int num;

            if(parameters != NULL) { num = atoi(parameters); } else { num = -1; }

            if(readNFiles(num, savedFileFolder) != 0) { // il lettura dei file è fallita
                return -1;
            }

            break;
        }

        case AC_DELETE: { // -c
            char* savePointer;
            char* nomeFile;

            int completed = 0;
            int total = 0;

            nomeFile = strtok_r(parameters, ",", &savePointer);
            while(nomeFile != NULL) {
                total++;
                if(removeFile(nomeFile) == 0) {
                    completed++;
                }
                
                nomeFile = strtok_r(NULL, ",", &savePointer);
            }

            if(total - completed != 0) {  // almeno un file non è stato eliminato
                return -1;
            }

            break;
        }

        case AC_ACQUIRE_MUTEX: { // -l
            char* savePointer;
            char* nomeFile;

            nomeFile = strtok_r(parameters, ",", &savePointer);
            while(nomeFile != NULL) {
                if(lockFile(nomeFile) != 0) {
                    return -1; // appena un file non viene letto do operazione fallita
                }
                
                nomeFile = strtok_r(NULL, ",", &savePointer);
            }

            break;
        }

        case AC_RELEASE_MUTEX: { // -u
            char* savePointer;
            char* nomeFile;

            nomeFile = strtok_r(parameters, ",", &savePointer);
            while(nomeFile != NULL) {
                if(unlockFile(nomeFile) != 0) {
                    return -1; // appena un file non viene letto do operazione fallita
                }
                closeFile(nomeFile);
                nomeFile = strtok_r(NULL, ",", &savePointer);
            }

            break;
        }
        
        default: {
            break;
        }
    }
    return 0;
}


int main(int argc, char* argv[]) {
	if(argc > 1) {
		int opt;
        
        ActionType listaAzioni[50];
        char listaParametri[50][1024];
        int actions = 0;
        DIR* cartella;
        strcpy(ejectedFileFolder, "#"); // imposta il valore iniziale su #
        strcpy(savedFileFolder, "#"); // imposta il valore iniziale su #

        // INIZIALIZZO FILE LOG
        fileLog = fopen("TestDirectory/output/Client/client_log.txt", "w+");
        checkStop(fileLog == NULL, "creazione file log");

		while ((opt = getopt(argc,argv, ":hf:w:W:D:r:R:d:t:l:u:c:p")) != -1) {
			switch(opt) {
				case 'h': // aiuto
                    help();
                break;
                
                case 'f': // imposta il socket a cui connettersi
                    socketPath = malloc(PATH_MAX);
                    memset(socketPath, 0, PATH_MAX);
                    strcpy(socketPath, optarg);
                    if(verbose) { ppf(CLR_HIGHLIGHT); printf("CLIENT> Socket impostato su %s millisecondi.\n", socketPath); ppff(); }
                break;

                case 'w': { // salva i file contenuti in una cartella su server
                    listaAzioni[actions] = AC_WRITE_RECU;
                    strcpy(listaParametri[actions], optarg);
                    actions++;
                    if(verbose) { ppf(CLR_INFO); printf("CLIENT> Operazione AC_WRITE_RECU (-w) in attesa.\n"); ppff(); }
                    break;
                }

                case 'W': { // salva i file separati da virgola
                    listaAzioni[actions] = AC_WRITE_LIST;
                    strcpy(listaParametri[actions], optarg);
                    actions++;
                    if(verbose) { ppf(CLR_INFO); printf("CLIENT> Operazione AC_WRITE_LIST (-W) in attesa.\n"); ppff(); }
                    break;
                }

                case 'D': { // cartella dove salvare file espulsi dal server (INSIEME A w|W!)
                    if((cartella = opendir(optarg)) == NULL) {
                        ppf(CLR_ERROR); printf("CLIENT> Cartella specificata '%s' inesistente (-D).\n", optarg); ppff();
                        free(cartella);
                        break;
                    }
                    free(cartella);
                    strcpy(ejectedFileFolder, optarg);
                    if(verbose) { ppf(CLR_HIGHLIGHT); printf("CLIENT> Cartella impostata su '%s' (-D).\n", optarg); ppff(); }
                    break;
                }

                case 'r': { // lista di file da leggere dal server separati da virgola
                    listaAzioni[actions] = AC_READ_LIST;
                    strcpy(listaParametri[actions], optarg);
                    actions++;
                    if(verbose) { ppf(CLR_INFO); printf("CLIENT> Operazione AC_READ_LIST in attesa.\n"); ppff(); }
                    break;
                }

                case 'R': { // legge n file qualsiasi dal server (se n=0 vengono letti tutti)
                    listaAzioni[actions] = AC_READ_RECU;
                    strcpy(listaParametri[actions], optarg);
                    actions++;
                    if(verbose) { ppf(CLR_INFO); printf("CLIENT> Operazione AC_READ_RECU in attesa.\n"); ppff(); }
                    break;
                } 

                case 'd': { // cartella dove salvare file letti con la r oppure R (INSIEME A r|R)
                    if((cartella = opendir(optarg)) == NULL) {
                        ppf(CLR_ERROR); printf("CLIENT> Cartella specificata '%s' inesistente (-d).\n", optarg); ppff();
                        free(cartella);
                        break;
                    }
                    free(cartella);
                    strcpy(savedFileFolder, optarg);
                    if(verbose) { ppf(CLR_HIGHLIGHT); printf("CLIENT> Cartella impostata su '%s' (-d).\n", optarg); ppff(); }
                    break;
                }

                case 't': { // specifica tempo tra una richiesta e l'altra in ms
                    timeoutRequests = atol(optarg);
                    if(verbose) { ppf(CLR_INFO); printf("CLIENT> Tempo tra una richiesta e l'altra: %ld\n", timeoutRequests); ppff(); }
                    break;
                }

                case 'l': { // lista di nomi di cui acquisire mutex
                    listaAzioni[actions] = AC_ACQUIRE_MUTEX;
                    strcpy(listaParametri[actions], optarg);
                    actions++;
                    if(verbose) { ppf(CLR_INFO); printf("CLIENT> Operazione AC_ACQUIRE_MUTEX in attesa.\n"); ppff(); }
                    break;
                }

                case 'u': { // lista di nomi di cui rilasciare mutex
                    listaAzioni[actions] = AC_RELEASE_MUTEX;
                    strcpy(listaParametri[actions], optarg);
                    actions++;
                    if(verbose) { ppf(CLR_INFO); printf("CLIENT> Operazione AC_RELEASE_MUTEX in attesa.\n"); ppff(); }
                    break;
                }

                case 'c': { // lista di file da rimuovere dal server
                    listaAzioni[actions] = AC_DELETE;
                    strcpy(listaParametri[actions], optarg);
                    actions++;
                    if(verbose) { ppf(CLR_INFO); printf("CLIENT> Operazione AC_DELETE in attesa.\n"); ppff(); }
                    break;
                }

                case 'p': { // attiva la modalità verbose che stampa roba
                    verbose = TRUE;
                    if(verbose) { ppf(CLR_INFO); printf("CLIENT> Verbose ATTIVATO.\n"); ppff(); }
                    break;
                }

				case ':': { // manca un argomento
					ppf(CLR_ERROR); printf("CLIENT> Argomento %c non valido\n", optopt); ppff();
				    break;
                }

				case '?': { // opzione non valida
					ppf(CLR_ERROR); printf("CLIENT> Argomento %c non riconosciuto\n", optopt); ppff();
				    break;
                }

				default:

                break;
			}
		}

        if(socketPath == NULL) {
            return -1;
        }
        

        int socketConnection;
        struct timespec tempoMassimo;
        tempoMassimo.tv_nsec = 0;
        tempoMassimo.tv_sec  = 10;
        

        if((socketConnection = openConnection(socketPath, 4999, tempoMassimo)) == -1) {
            // connessione fallita
            pe("CLIENT> Errore durante la connessione");
            free(socketPath);
            return -1;
        }

        

        // connessione stabilita
        struct sigaction s;
        memset(&s, 0, sizeof(s));
        s.sa_handler = SIG_IGN;
        // ignoro SIGPIPE per evitare di essere terminato da una scrittura su un socket chiuso
        if ((sigaction(SIGPIPE, &s, NULL)) == -1) {
            pe("Errore sigaction");
            free(socketPath);
            return -1;
        }

        // === INIZIO COMUNICAZIONE CON SERVER ===
        // creo variabile per contenere richieste
        Message* msg = calloc(4, sizeof(Message));
        checkStop(msg == NULL, "malloc msg");
        // fine variabile per contenere richieste
        setMessage(msg, ANS_UNKNOWN, 0, NULL, NULL, 0);

        // attendo il messaggio di benvenuto dal server
        int esito = readMessage(socketConnection, msg);
        ActionType ac = msg->action;
        if(esito == 0) {
            if(ac == ANS_WELCOME) {
                ppf(CLR_SUCCESS); printSave("CLIENT> Ricevuto WELCOME dal server: %s", msg->data); ppff();
                free(msg->data);

                char* testo_msg = "Grazie, ci sono";
                setMessage(msg, ANS_HELLO, 0, NULL, testo_msg, strlen(testo_msg));

                ppf(CLR_INFO); printSave("CLIENT> Rispondo al WELCOME con: %s", testo_msg);

                checkStop(sendMessage(socketConnection, msg) != 0, "msg hello al server iniziale");

                ppf(CLR_SUCCESS); printSave("CLIENT> Inviato HELLO al server con successo!"); ppff();
            } else if(ac == ANS_MAX_CONN_REACHED) {
                ppf(CLR_ERROR); printf("CLIENT> Il server ha raggiunto il limite massimo di connessioni.\n"); ppff();
            }
        }

        
        free(msg);

        // se l'ultima richiesta è stata una WELCOME allora inizio ad eseguire le richieste
        if(ac == ANS_WELCOME) {
            // eseguo tutte le azioni richieste
            for(int i = 0; i < actions; i++) {
                if(executeAction(listaAzioni[i], listaParametri[i]) == 0) {
                    ppf(CLR_SUCCESS); printf("CLIENT> Operazione n°%d completata con successo.\n", listaAzioni[i]); ppff();
                    
					struct timespec t;
					t.tv_sec  = (int)timeoutRequests/1000;
					t.tv_nsec = (timeoutRequests%1000)*1000000000;
					nanosleep(&t, NULL);
                } else {
                    ppf(CLR_ERROR); printf("CLIENT> Operazione n°%d fallita. Motivo: %s (codice errore %d)\n", listaAzioni[i], strerror(errno), errno); ppff();
                }
            }
        }


        // finite le azioni richieste, chiudo la connessione
		if(closeConnection(socketPath) == 0) {
            ppf(CLR_INFO); printf("CLIENT> Connessione terminata con successo.\n"); ppff();
        } else {
            ppf(CLR_ERROR); printf("CLIENT> Connessione non terminata.\n"); ppff();
        }

        free(socketPath);

		return 0;
	} else {
		ppf(CLR_ERROR); printf("CLIENT> Fornisci almeno un argomento. Usa -h se non sei sicuro.\n"); ppff();
		return 0;
	}
    return 0;
}
