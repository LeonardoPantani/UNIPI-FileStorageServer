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

#include "api.c"
#include "utils/includes/message.c"
#include "communication.c"

int verbose = FALSE; // stampa per ogni operazione

long timeoutRequests = 0; // tempo tra una richiesta e l'altra

char ejectedFileFolder[PATH_MAX]; // -D percorso dove salvare file espulsi dal server (serve w|W)

char savedFileFolder[PATH_MAX]; // -d percorso dove salvare i file letti dal server (serve r|R)

pthread_mutex_t mutexChiusura = PTHREAD_MUTEX_INITIALIZER;
int chiusuraForte = FALSE;

void help() {
    ppf(CLR_HIGHLIGHT); printf("Utilizzo:\n"); ppff();
    pp("    -h                 \033[0m->\033[94m stampa questa lista", CLR_INFO);
    pp("    -f <filename>      \033[0m->\033[94m specifica il nome del socket a cui connettersi", CLR_INFO);
    pp("    -w <dirname[,n=0]> \033[0m->\033[94m invia i file nella cartella dirname; se n=0 manda tutti i file, altrimenti manda n file", CLR_INFO);
    pp("    -W <file1[,file2]> \033[0m->\033[94m lista di file da inviare al server", CLR_INFO);
    pp("    -D <dirname>       \033[0m->\033[94m cartella dove mettere i file in caso vengano espulsi dal server", CLR_INFO);
    pp("    -r <file1[,file2]> \033[0m->\033[94m lista di file richiesti al server", CLR_INFO);
    pp("    -R [n=0]           \033[0m->\033[94m legge n file dal server, se n non specificato li legge tutti", CLR_INFO);
    pp("    -d <dirname>       \033[0m->\033[94m cartella dove mettere i file letti dal server", CLR_INFO);
    pp("    -t time            \033[0m->\033[94m tempo (ms) che intercorre tra una richiesta di connessione e l'altra al server", CLR_INFO);
    pp("    -l <file1[,file2]> \033[0m->\033[94m lista di file da bloccare", CLR_INFO);
    pp("    -u <file1[,file2]> \033[0m->\033[94m lista di file da sbloccare", CLR_INFO);
    pp("    -c <file1[,file2]> \033[0m->\033[94m lista di file da rimuovere dal server", CLR_INFO);
    pp("    -p                 \033[0m->\033[94m abilita le stampe sull'stdout di ogni operazione", CLR_INFO);
}

static int sendFilesList(char* nomeCartella, int numFiles, int completed, int total) {
    if(numFiles > 0 || numFiles == -1) {
        DIR* cartella;
        struct dirent *entry;

        if(!(cartella = opendir(nomeCartella))) return -1;

        while((entry = readdir(cartella)) != NULL && (numFiles > 0 || numFiles == -1)) {
            char path[PATH_MAX];
            if(entry->d_type == DT_DIR) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                    continue;
                snprintf(path, sizeof(path), "%s/%s", nomeCartella, entry->d_name);
                sendFilesList(path, numFiles, completed, total);
            } else { // è un file
                snprintf(path, sizeof(path), "%s/%s", nomeCartella, entry->d_name);
                struct stat proprieta;
                int esito = stat(path, &proprieta);
                
                if(openFile(path, O_CREATE|O_LOCK) == 0) { // chiamata alla funzione che apre il file e lo manda
                    completed++;
                }
                total++;
                
                if(numFiles != -1) numFiles--;
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
                ppf(CLR_ERROR); printf("CLIENT> Cartella specificata '%s' inesistente.\n", nomeCartella); ppff();
                return -1;
            } else {
                ppf(CLR_HIGHLIGHT); printf("CLIENT> Cartella per la write impostata su '%s'", nomeCartella);
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
                } else {
                    pp("CLIENT> Specificato sottoargomento 'n' ma non il suo valore.", CLR_ERROR);
                    return -1;
                }
            } else {
                printf(" | numero file: TUTTI\n"); ppff();
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
                        if(openFile(nomeFile, O_CREATE|O_LOCK) == 0) {
                            completed++;
                        }
                    } else {
                        ppf(CLR_ERROR); printf("CLIENT> '%s' è una cartella.\n", nomeFile); ppff();
                    }
                } else {
                    ppf(CLR_ERROR); printf("CLIENT> Il file '%s' non esiste o è corrotto.\n", nomeFile); ppff();
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

            struct stat checkfile;

            int numFiles = -1;
            int saveFiles = TRUE;

            if(strcmp(savedFileFolder, "#") == 0) {
                ppf(CLR_WARNING); printf("CLIENT> I file non saranno salvati perché non hai specificato nessuna cartella di destinazione con -d.\n"); ppff();
                saveFiles = FALSE;
            }
            
            nomeFile = strtok_r(parameters, ",", &savePointer);
            while(nomeFile != NULL) {
                if(readFile(nomeFile, &puntatoreFile, &dimensioneFile) == 0) {
                    if(saveFiles) { // effettuo operazioni sottostanti solo se i file vanno salvati
                        FILE* filePointer;
                        char percorso[PATH_MAX];

                        memset(percorso, 0, strlen(percorso));

                        strcpy(percorso, savedFileFolder);
                                        
                        if(percorso[strlen(percorso) - 1] != '/') {
                            strcat(percorso, "/");
                        }
                        strcat(percorso, basename(nomeFile));

                        filePointer = fopen(percorso, "wb");
                        if(filePointer == NULL) { pe("File non aperto!!"); }

                        fwrite(puntatoreFile, 1, dimensioneFile, filePointer);
                        ppf(CLR_INFO); printf("CLIENT> File '%s' salvato in '%s'\n", nomeFile, percorso); ppff();
                                        
                        fclose(filePointer);
                    }
                } else {
                    return -1; // appena un file non viene letto do operazione fallita
                }
                
                nomeFile = strtok_r(NULL, ",", &savePointer);
            }
        }

        case AC_READ_RECU: {
            if(readNFiles(atoi(parameters), savedFileFolder) != 0) { // il lettura dei file è fallita
                return -1;
            }
        }

        case AC_DELETE: { // temp: non cancella, chiude e basta (test)
            char* savePointer;
            char* nomeFile;

            int completed = 0;
            int total = 0;

            nomeFile = strtok_r(parameters, ",", &savePointer);
            while(nomeFile != NULL) {
                total++;
                if(closeFile(nomeFile) == 0) {
                    completed++;
                }
                
                nomeFile = strtok_r(NULL, ",", &savePointer);
            }

            if(total - completed != 0) {  // almeno un file non è stato chiuso
                return -1;
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
        strcpy(ejectedFileFolder, "#"); // imposta il valore iniziale su #
        strcpy(savedFileFolder, "#"); // imposta il valore iniziale su #

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
                    if(verbose) { pp("CLIENT> Operazione AC_WRITE (-w) in attesa.", CLR_INFO); }
                    break;
                }

                case 'W': { // salva i file separati da virgola
                    listaAzioni[actions] = AC_WRITE_LIST;
                    strcpy(listaParametri[actions], optarg);
                    actions++;
                    if(verbose) { pp("CLIENT> Operazione AC_WRITE_LIST (-W) in attesa.", CLR_INFO); }
                    break;
                }

                case 'D': { // cartella dove salvare file espulsi dal server (INSIEME A w|W!)
                    DIR* cartella;
                    if((cartella = opendir(optarg)) == NULL) {
                        ppf(CLR_ERROR); printf("CLIENT> Cartella specificata '%s' inesistente (-D).\n", optarg); ppff();
                        break;
                    }
                    strcpy(ejectedFileFolder, optarg);
                    if(verbose) { ppf(CLR_HIGHLIGHT); printf("CLIENT> Cartella impostata su '%s' (-D).\n", optarg); ppff(); }
                    break;
                }

                case 'r': { // lista di file da leggere dal server separati da virgola
                    listaAzioni[actions] = AC_READ_LIST;
                    strcpy(listaParametri[actions], optarg);
                    actions++;
                    if(verbose) { pp("CLIENT> Operazione AC_READ_LIST in attesa.", CLR_INFO); }
                    break;
                }

                case 'R': { // legge n file qualsiasi dal server (se n=0 vengono letti tutti)
                    listaAzioni[actions] = AC_READ_RECU;
                    strcpy(listaParametri[actions], optarg);
                    actions++;
                    if(verbose) { pp("CLIENT> Operazione AC_READ_RECU in attesa.", CLR_INFO); }
                    break;
                } 

                case 'd': { // cartella dove salvare file letti con la r oppure R (INSIEME A r|R)
                    DIR* cartella;
                    if((cartella = opendir(optarg)) == NULL) {
                        ppf(CLR_ERROR); printf("CLIENT> Cartella specificata '%s' inesistente (-d).\n", optarg); ppff();
                        break;
                    }
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
                    if(verbose) { pp("CLIENT> Operazione AC_ACQUIRE_MUTEX in attesa.", CLR_INFO); }
                    break;
                }

                case 'u': { // lista di nomi di cui rilasciare mutex
                    listaAzioni[actions] = AC_RELEASE_MUTEX;
                    strcpy(listaParametri[actions], optarg);
                    actions++;
                    if(verbose) { pp("CLIENT> Operazione AC_RELEASE_MUTEX in attesa.", CLR_INFO); }
                    break;
                }

                case 'c': { // lista di file da rimuovere dal server
                    listaAzioni[actions] = AC_DELETE;
                    strcpy(listaParametri[actions], optarg);
                    actions++;
                    if(verbose) { pp("CLIENT> Operazione AC_DELETE in attesa.", CLR_INFO); }
                    break;
                }

                case 'p': { // attiva la modalità verbose che stampa roba
                    verbose = TRUE;
                    if(verbose) { ppf(CLR_INFO); printf("CLIENT> Verbose ATTIVATO.\n"); ppff(); }
                    break;
                }

				case ':': { // manca un argomento
					ppf(CLR_ERROR); printf("Argomento %c non valido\n", optopt); ppff();
				    break;
                }

				case '?': { // opzione non valida
					ppf(CLR_ERROR); printf("Argomento %c non riconosciuto\n", optopt); ppff();
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
        

        if((socketConnection = openConnection(socketPath, 5000, tempoMassimo)) == -1) {
            // connessione fallita
            pe("Errore durante la connessione");
            return -1;
        }

        

        // connessione stabilita
        struct sigaction s;
        memset(&s, 0, sizeof(s));
        s.sa_handler = SIG_IGN;
        // ignoro SIGPIPE per evitare di essere terminato da una scrittura su un socket chiuso
        if ((sigaction(SIGPIPE, &s, NULL)) == -1) {
            pe("Errore sigaction");
            return -1;
        }

        // === INIZIO COMUNICAZIONE CON SERVER ===
        // creo variabile per contenere richieste
        Message* msg = malloc(sizeof(Message));
        checkStop(msg == NULL, "malloc msg");
        // fine variabile per contenere richieste
        setMessage(msg, AC_UNKNOWN, 0, NULL, NULL, 0);


        // attendo il messaggio di benvenuto dal server
        int esito = readMessage(socketConnection, msg);
        if(esito == 0) {
            if(msg->action == AC_WELCOME) {
                ppf(CLR_SUCCESS); printf("CLIENT> Ricevuto WELCOME dal server: %s\n", msg->data); ppff();

                char* testo_msg = "Grazie, ci sono";
                setMessage(msg, AC_HELLO, 0, NULL, testo_msg, strlen(testo_msg));

                ppf(CLR_INFO); printf("CLIENT> Rispondo al WELCOME con: %s\n", testo_msg);

                checkStop(sendMessage(socketConnection, msg) != 0, "msg hello al server iniziale");

                pp("CLIENT> Inviato HELLO al server con successo!", CLR_SUCCESS);
            } else if(msg->action == AC_MAXCONNECTIONSREACHED) {
                ppf(CLR_ERROR); printf("CLIENT> Il server ha raggiunto il limite massimo di connessioni.\n"); ppff();
            }
        }

        // se l'ultima richiesta è stata una HELLO (inviata da me), allora posso iniziare ad eseguire le richieste
        if(msg->action == AC_HELLO) {
            // eseguo tutte le azioni richieste
            for(int i = 0; i < actions; i++) {
                if(executeAction(listaAzioni[i], listaParametri[i]) == 0) {
                    ppf(CLR_SUCCESS); printf("CLIENT> Operazione n°%d completata con successo.\n", listaAzioni[i]); ppff();
                } else {
                    ppf(CLR_ERROR); printf("CLIENT> Operazione n°%d fallita. Esco!\n", listaAzioni[i]); ppff();
                    break;
                }

                // sleep(3);
            }
        }


        // finite le azioni richieste, chiudo la connessione
		if(closeConnection(socketPath) == 0) {
            pp("CLIENT> Connessione terminata con successo.", CLR_INFO);
        } else {
            pp("CLIENT> Connessione non terminata!!", CLR_ERROR);
        }
		return 0;
	} else {
		pp("Fornisci almeno un argomento. Usa -h se non sei sicuro.", CLR_ERROR);
		return 0;
	}
    return 0;
}