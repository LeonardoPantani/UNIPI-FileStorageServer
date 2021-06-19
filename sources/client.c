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

FILE* fileLog; // puntatore al file di log del client
pthread_mutex_t mutexFileLog = PTHREAD_MUTEX_INITIALIZER;

int verbose = FALSE; // stampa per ogni operazione

long timeoutRequests = 0; // tempo tra una richiesta e l'altra

char ejectedFileFolder[PATH_MAX]; // -D savedFilePath dove salvare file espulsi dal server (serve w|W)

char savedFileFolder[PATH_MAX]; // -d savedFilePath dove salvare i file letti dal server (serve r|R)

/**
 * @brief   Controlla se i parametri ricevuti in ingresso sono validi.
 * @note    Funzione interna.
 * 
 * @param   actionList      Lista di azioni
 * @param   parameterList   Lista di parametri
 * @param   maxActions      Azioni massime
 * 
 * @returns 0 se va tutto bene, -1 se un parametro non rispetta le specifiche
**/
static int checkProgramParameters(ActionType actionList[], int totalActions, char eff[], char sff[]) {
    int ok = 0;
    int required_R = 0, required_W = 0;

    if(strcmp(eff, "#") != 0) required_R = 1;
    if(strcmp(sff, "#") != 0) required_W = 1;

    for(int i = 0; i < totalActions && (required_R == 1 || required_W == 1); i++) {
        if(required_R == 1 && (actionList[i] == AC_READ_LIST || actionList[i] == AC_READ_RECU))
            required_R = -1;
        if(required_W == 1 && (actionList[i] == AC_WRITE_LIST || actionList[i] == AC_WRITE_RECU))
            required_W = -1;
    }

    // sono stati cercati e non sono stati trovati
    if(required_R == 1) {
        ok = -1;
        ppf(CLR_IMPORTANT); printf("CLIENT> L'opzione -d richiede almeno un'operazione dei tipi: -r oppure -R\n"); ppff();
    } 
    if(required_W == 1) {
        ok = -1;
        ppf(CLR_IMPORTANT); printf("CLIENT> L'opzione -D richiede almeno un'operazione dei tipi: -w oppure -W\n"); ppff();
    } 

    return ok;
}

/**
 * @brief   Stampa la schermata di aiuto del client.
**/
static void help(void) {
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

/**
 * @brief   Scrive un file al percorso specificato sul server.
 * @note    Se un file non viene creato allora si prova ad aprirlo e basta.
 *          In entrambi i casi si prova a scrivere il contenuto di filePath e poi si chiude il file remoto.
 * 
 * @param   filePath    Percorso del file da scrivere
 * @param   properties  Proprietà del file
 * @param   ejectedDest Cartella di destinazione dei file ricevuti dal server in caso di espulsione
**/
static int writeOnFile(char* filePath, struct stat properties, char ejectedDest[]) {
    if(openFile(filePath, O_CREATE) == 0) { // se il file non esiste lo creo, ci scrivo e lo chiudo
        if(writeFile(filePath, ejectedDest) == 0) { 
            if(closeFile(filePath) == 0) {
                return 1;
            }
        }
    } else if(openFile(filePath, 0) == 0) { // se il file esiste già allora lo apro, ci scrivo in append e lo chiudo
        FILE* filePointer;
        filePointer = fopen(filePath, "rb");
        checkStop(filePointer == NULL, "file rilevato nella cartella non aperto");

        void* data = cmalloc(sizeof(char)*properties.st_size);

        if(fread(data, 1, properties.st_size, filePointer) == properties.st_size) { // se la lettura ha successo
            if(appendToFile(filePath, data, properties.st_size, ejectedDest) == 0) {
                if(closeFile(filePath) == 0) {
                    fclose(filePointer);
                    free(data);
                    return 1;
                }
            }
        }

        fclose(filePointer);
        free(data);
    }

    return 0;
}

/**
 * @brief   Scrive un insieme di file contenuti in una cartella sul server.
 * @note    Ricorsiva
 * 
 * @param   folderPath  Percorso della cartella da cui prendere i file
 * @param   numFiles    Numero di file da inviare
 * @param   completed   (interno) conta i file inviati
 * @param   total       (interno) conta i file totali
**/
static int sendFilesList(char* folderPath, int numFiles, int completed, int total) {
    if(numFiles > 0 || numFiles == -1) {
        DIR* folder;
        struct dirent *entry;

        if(!(folder = opendir(folderPath))) return -1;

        while((entry = readdir(folder)) != NULL && (numFiles > 0 || numFiles == -1)) {
            char path[PATH_MAX];
            if(folderPath[strlen(folderPath)-1] != '/')
                snprintf(path, sizeof(path), "%s/%s", folderPath, entry->d_name);
            else
                snprintf(path, sizeof(path), "%s%s", folderPath, entry->d_name);
            
            struct stat properties;

            if(stat(path, &properties) == 0) {
                if(S_ISDIR(properties.st_mode)) {
                    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                        continue;
                    sendFilesList(path, numFiles, completed, total);
                } else { // è un file
                    completed += writeOnFile(path, properties, ejectedFileFolder);
                    total++;
                    
                    if(numFiles != -1) numFiles--;
                }
            }
        }
        closedir(folder);
    }

    return total-completed;
}

/**
 * @brief   Funzione chiave, esegue una azione in base a determinati parametri.
 * 
 * @param ac            Codice identificativo dell'azione da elaborare
 * @param parameters    Parametri passati all'azione
**/
static int executeAction(ActionType ac, char* parameters) {
    char* savePointer;
    char* filePath;
    char* folderPath;

    switch(ac) {
        case AC_WRITE_RECU: { // -w
            DIR* folder;

            int numFiles = -1;
            
            folderPath = strtok_r(parameters, ",", &savePointer);
            if((folder = opendir(folderPath)) == NULL) {
                ppf(CLR_ERROR); printSave("CLIENT> Cartella specificata '%s' inesistente.", folderPath); ppff();
                free(folder);
                return -1;
            } else {
                ppf(CLR_HIGHLIGHT); printf("CLIENT> Cartella per la write impostata su '%s'", folderPath); fflush(stdout);
                free(folder);
            }
            char* temp = strtok_r(NULL, ",", &savePointer);

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

            return sendFilesList(folderPath, numFiles, 0, 0); // restituisce la differenza tra file totali e file inviati con successo (se 0 ok, !=0 non tutti inviati)
        }

        case AC_WRITE_LIST: { // -W
            int completed = 0;
            int total = 0;

            filePath = strtok_r(parameters, ",", &savePointer);
            while(filePath != NULL) {
                total++;

                struct stat properties;

                if(stat(filePath, &properties) == 0) {
                    if(S_ISDIR(properties.st_mode) == 0) {
                        completed += writeOnFile(filePath, properties, ejectedFileFolder);
                    } else {
                        ppf(CLR_ERROR); printSave("CLIENT> '%s' è una cartella.", filePath); ppff();
                    }
                } else {
                    ppf(CLR_ERROR); printSave("CLIENT> Il file '%s' non esiste o è corrotto.", filePath); ppff();
                }

                filePath = strtok_r(NULL, ",", &savePointer);
            }

            if(total - completed != 0) { // almeno un file non è stato inviato
                return -1;
            }

            break;
        }

        case AC_READ_LIST: { // -r
            void* filePointer;
            size_t fileSize;

            int saveFiles = TRUE;

            if(strcmp(savedFileFolder, "#") == 0) {
                ppf(CLR_WARNING); printSave("CLIENT> I file non saranno salvati perché non hai specificato nessuna cartella di destinazione con -d."); ppff();
                saveFiles = FALSE;
            }
            
            filePath = strtok_r(parameters, ",", &savePointer);
            while(filePath != NULL) {
                if(readFile(filePath, &filePointer, &fileSize) == 0) {
                    if(saveFiles) { // effettuo operazioni sottostanti solo se i file vanno salvati
                        FILE* file;
                        char savedFilePath[PATH_MAX];

                        memset(savedFilePath, 0, PATH_MAX);

                        strcpy(savedFilePath, savedFileFolder);
                                        
                        if(savedFilePath[PATH_MAX - 1] != '/') {
                            strcat(savedFilePath, "/");
                        }
                        strcat(savedFilePath, basename(filePath));

                        file = fopen(savedFilePath, "wb");
                        checkStop(file == NULL, "file rilevato nella cartella non aperto");

                        fwrite(filePointer, 1, fileSize, file);
                        ppf(CLR_INFO); printSave("CLIENT> File '%s' salvato in '%s'", filePath, savedFilePath); ppff();
 
                        free(filePointer);
                        fclose(file);
                    }
                } else {
                    return -1; // appena un file non viene letto do operazione fallita
                }
                
                filePath = strtok_r(NULL, ",", &savePointer);
            }

            break;
        }

        case AC_READ_RECU: { // -R
            int num;

            if(parameters != NULL) { num = atoi(parameters); } else { num = -1; }

            if(readNFiles(num, savedFileFolder) != 0) { // lettura dei file fallita
                return -1;
            }

            break;
        }

        case AC_DELETE: { // -c
            int completed = 0;
            int total = 0;

            filePath = strtok_r(parameters, ",", &savePointer);
            while(filePath != NULL) {
                total++;
                if(removeFile(filePath) == 0) {
                    completed++;
                }
                
                filePath = strtok_r(NULL, ",", &savePointer);
            }

            if(total - completed != 0) {  // almeno un file non è stato eliminato
                return -1;
            }

            break;
        }

        case AC_ACQUIRE_MUTEX: { // -l
            filePath = strtok_r(parameters, ",", &savePointer);
            while(filePath != NULL) {
                if(lockFile(filePath) != 0) {
                    return -1; // appena un file non viene letto do operazione fallita
                }
                
                filePath = strtok_r(NULL, ",", &savePointer);
            }

            break;
        }

        case AC_RELEASE_MUTEX: { // -u
            filePath = strtok_r(parameters, ",", &savePointer);
            while(filePath != NULL) {
                if(unlockFile(filePath) != 0) {
                    return -1; // appena un file non viene letto do operazione fallita
                }
                closeFile(filePath);
                filePath = strtok_r(NULL, ",", &savePointer);
            }

            break;
        }
        
        default: {
            ppf(CLR_ERROR); printSave("CLIENT> Operazione n°%d non ancora supportata.", ac); ppff();
            break;
        }
    }
    return 0;
}


int main(int argc, char* argv[]) {
	if(argc <= 1) {
        ppf(CLR_ERROR); printf("CLIENT> Fornisci almeno un argomento; usa -h se non sei sicuro\n"); ppff();
        return -1;
    }

    int opt;
    
    ActionType actionList[MAX_CLIENT_ACTIONS];
    char parameterList[MAX_CLIENT_ACTIONS][PATH_MAX];

    int actions = 0;

    DIR* folder;
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
                socketPath = cmalloc(PATH_MAX);
                memset(socketPath, 0, PATH_MAX);
                strcpy(socketPath, optarg);
                if(verbose) { ppf(CLR_HIGHLIGHT); printSave("CLIENT> Socket impostato su %s.", socketPath); ppff(); }
            break;

            case 'w': { // salva i file contenuti in una cartella su server
                actionList[actions] = AC_WRITE_RECU;
                strcpy(parameterList[actions], optarg);
                actions++;
                if(verbose) { ppf(CLR_INFO); printSave("CLIENT> Operazione AC_WRITE_RECU (-w) in attesa."); ppff(); }
                break;
            }

            case 'W': { // salva i file separati da virgola
                actionList[actions] = AC_WRITE_LIST;
                strcpy(parameterList[actions], optarg);
                actions++;
                if(verbose) { ppf(CLR_INFO); printSave("CLIENT> Operazione AC_WRITE_LIST (-W) in attesa."); ppff(); }
                break;
            }

            case 'D': { // cartella dove salvare file espulsi dal server (INSIEME A w|W!)
                if((folder = opendir(optarg)) == NULL) {
                    ppf(CLR_ERROR); printSave("CLIENT> Cartella specificata '%s' inesistente (-D).", optarg); ppff();
                    free(folder);
                    break;
                }
                closedir(folder);
                strcpy(ejectedFileFolder, optarg);
                if(verbose) { ppf(CLR_HIGHLIGHT); printSave("CLIENT> Cartella impostata su '%s' (-D).", optarg); ppff(); }
                break;
            }

            case 'r': { // lista di file da leggere dal server separati da virgola
                actionList[actions] = AC_READ_LIST;
                strcpy(parameterList[actions], optarg);
                actions++;
                if(verbose) { ppf(CLR_INFO); printSave("CLIENT> Operazione AC_READ_LIST (-r) in attesa."); ppff(); }
                break;
            }

            case 'R': { // legge n file qualsiasi dal server (se n=0 vengono letti tutti)
                actionList[actions] = AC_READ_RECU;
                strcpy(parameterList[actions], optarg);
                actions++;
                if(verbose) { ppf(CLR_INFO); printSave("CLIENT> Operazione AC_READ_RECU (-R *n*) in attesa."); ppff(); }
                break;
            } 

            case 'd': { // cartella dove salvare file letti con la r oppure R (INSIEME A r|R!)
                if((folder = opendir(optarg)) == NULL) {
                    ppf(CLR_ERROR); printSave("CLIENT> Cartella specificata '%s' inesistente (-d).", optarg); ppff();
                    free(folder);
                    break;
                }
                closedir(folder);
                strcpy(savedFileFolder, optarg);
                if(verbose) { ppf(CLR_HIGHLIGHT); printSave("CLIENT> Cartella impostata su '%s' (-d).", optarg); ppff(); }
                break;
            }

            case 't': { // specifica tempo tra una richiesta e l'altra in ms
                timeoutRequests = atol(optarg);
                if(verbose) { ppf(CLR_INFO); printSave("CLIENT> Tempo tra una richiesta e l'altra: %ld (-t).", timeoutRequests); ppff(); }
                break;
            }

            case 'l': { // lista di nomi di cui acquisire mutex | NON IMPL.
                actionList[actions] = AC_ACQUIRE_MUTEX;
                strcpy(parameterList[actions], optarg);
                actions++;
                if(verbose) { ppf(CLR_INFO); printSave("CLIENT> Operazione AC_ACQUIRE_MUTEX (-l) in attesa."); ppff(); }
                break;
            }

            case 'u': { // lista di nomi di cui rilasciare mutex | NON IMPL.
                actionList[actions] = AC_RELEASE_MUTEX;
                strcpy(parameterList[actions], optarg);
                actions++;
                if(verbose) { ppf(CLR_INFO); printSave("CLIENT> Operazione AC_RELEASE_MUTEX (-u) in attesa."); ppff(); }
                break;
            }

            case 'c': { // lista di file da rimuovere dal server
                actionList[actions] = AC_DELETE;
                strcpy(parameterList[actions], optarg);
                actions++;
                if(verbose) { ppf(CLR_INFO); printSave("CLIENT> Operazione AC_DELETE (-c) in attesa."); ppff(); }
                break;
            }

            case 'p': { // attiva la modalità verbose che stampa roba
                verbose = TRUE;
                if(verbose) { ppf(CLR_INFO); printSave("CLIENT> Verbose ATTIVATO (-p)."); ppff(); }
                break;
            }

            case ':': { // manca un argomento
                switch(optopt) {
                    case 'R': {
                        actionList[actions] = AC_READ_RECU;
                        strcpy(parameterList[actions], "0");
                        actions++;
                        if(verbose) { ppf(CLR_INFO); printSave("CLIENT> Operazione AC_READ_RECU (-R *0*) in attesa."); ppff(); }
                        break;
                    }

                    default: {
                        ppf(CLR_ERROR); printSave("CLIENT> Argomento %c non valido", optopt); ppff();
                    }
                }
                break;
            }

            case '?': { // opzione non valida
                ppf(CLR_ERROR); printSave("CLIENT> Argomento %c non riconosciuto", optopt); ppff();
                break;
            }

            default:

            break;
        }
    }

    // controllo che i parametri -d e -D siano accompagnati dalle relative richieste
    if(checkProgramParameters(actionList, actions, ejectedFileFolder, savedFileFolder) != 0) {
        ppf(CLR_ERROR); printSave("CLIENT> Alcuni parametri forniti non sono validi, ulteriori dettagli specificati sopra. Riprova."); ppff();
        return -1;
    }

    checkM1(socketPath == NULL, "percorso socket nullo");
    

    int socketConnection;

    struct timespec maxTimeout; maxTimeout.tv_nsec = 0; maxTimeout.tv_sec  = 10;

    if((socketConnection = openConnection(socketPath, 1000, maxTimeout)) == -1) { // connessione fallita
        pe("CLIENT> Errore durante la connessione");
        fclose(fileLog);
        free(socketPath);
        return -1;
    }

    

    // connessione stabilita
    struct sigaction s;
    memset(&s, 0, sizeof(s));
    s.sa_handler = SIG_IGN;
    // ignoro SIGPIPE per evitare di essere terminato da una scrittura su un socket chiuso
    if ((sigaction(SIGPIPE, &s, NULL)) == -1) {
        pe("CLIENT> Errore sigaction");
        fclose(fileLog);
        free(socketPath);
        return -1;
    }

    // === INIZIO COMUNICAZIONE CON SERVER ===
    // creo variabile per contenere richieste
    Message* msg = cmalloc(sizeof(Message));
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
            ppf(CLR_ERROR); printSave("CLIENT> Il server ha raggiunto il limite massimo di connessioni."); ppff();
        } else {
            ppf(CLR_ERROR); printSave("CLIENT> Il server ha mandato una risposta non valida. ACTION: %d", ac); ppff();
        }
    }
    free(msg);

    // imposto tempo tra una richiesta e l'altra
    struct timespec t;
    t.tv_sec  = (int)timeoutRequests/1000;
    t.tv_nsec = (timeoutRequests%1000)*1000000000;

    // se l'ultima richiesta è stata una WELCOME allora inizio ad eseguire le richieste
    if(ac == ANS_WELCOME) {
        // eseguo tutte le azioni richieste
        for(int i = 0; i < actions; i++) {
            if(executeAction(actionList[i], parameterList[i]) == 0) {
                ppf(CLR_SUCCESS); printSave("CLIENT> Operazione %s completata con successo", getOperationName(actionList[i])); ppff();
                
                if(i+1 < actions) nanosleep(&t, NULL); // aspetto solo se ho ancora operazioni da eseguire
            } else {
                ppf(CLR_ERROR); printSave("CLIENT> Operazione %s fallita, motivo: %s (codice errore %d)", getOperationName(actionList[i]), strerror(errno), errno); ppff();
            }
        }
    }


    // finite le azioni richieste, chiudo la connessione
    if(closeConnection(socketPath) == 0) {
        ppf(CLR_INFO); printSave("CLIENT> Connessione terminata con successo"); ppff();
    } else {
        ppf(CLR_ERROR); printSave("CLIENT> Connessione non terminata"); ppff();
    }

    fclose(fileLog);
    free(socketPath);

    return 0;
}