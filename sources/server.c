/**
 * @file    server_so.c
 * @brief   Contiene l'implementazione del server, tra cui caricamento dei dati config, salvataggio statistiche ed esecuzione delle richieste.
 * @author  Leonardo Pantani
**/

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "config.h"
#include "file.h"
#include "hashtable.h"
#include "communication.h"
#include "list.h"
#include "statistics.h"

FILE* fileLog;
pthread_mutex_t mutexFileLog = PTHREAD_MUTEX_INITIALIZER;

Statistics stats = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
pthread_mutex_t mutexStatistiche = PTHREAD_MUTEX_INITIALIZER;

hashtable_t *ht = NULL;
pthread_mutex_t* mutexHT = NULL;
int nmutex = 0;

List *coda_client = NULL;
pthread_mutex_t mutexCodaClient = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t clientInAttesa = PTHREAD_COND_INITIALIZER;

int chiusuraForte  = FALSE;
int chiusuraDebole = FALSE;
pthread_mutex_t mutexChiusura = PTHREAD_MUTEX_INITIALIZER;


char* findOldFile(hashtable_t hasht) {
	hash_elem_it it2 = HT_ITERATOR(ht);
	char* k = ht_iterate_keys(&it2);

    return k;
}


static int checkLimits(Message* msg, int which) {
    if(stats.current_bytes_used + msg->data_length > config.max_memory_size && (which == 1 || which == 3)) { // l'inserimento del file causerebbe sforamento limiti memoria
        return -1;
    } else if(stats.current_files_saved + 1 > config.max_files && (which == 2 || which == 3)) { // l'inserimento del file causerebbe sforamento limiti quantità file
        return -2;
    } else { // l'inserimento del file non causerebbe problemi
        return 0;
    }
}


int expellFiles(Message* msg, int socketConnection, int numGestoreConnessione, int controllo) {
    int i = 0;

    // creo variabile per contenere richieste
    Message* risposta = cmalloc(sizeof(Message));
    checkStop(risposta == NULL, "malloc risposta");
    // fine variabile per contenere richieste

    setMessage(risposta, ANS_UNKNOWN, 0, NULL, NULL, 0);

    int esitoControllo = checkLimits(msg, controllo);
    
    if(esitoControllo != 0) {
        ppf(CLR_HIGHLIGHT); printSave("GC %d > Controllo limitazioni con ID %d", numGestoreConnessione, controllo); ppff();
        setMessage(risposta, ANS_STREAM_START, 0, NULL, NULL, 0);
        checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio inizio flush");

        do {
            char* chiave = findOldFile(*ht);
            File* oldFile = ht_get(ht, chiave);
            
            // aggiornamento statistiche
            locka(mutexStatistiche);
            stats.current_files_saved--;
            stats.current_bytes_used = stats.current_bytes_used - oldFile->data_length;
            stats.n_replace_applied++;
            unlocka(mutexStatistiche);
            
            ppf(CLR_ERROR); printSave("GC %d > File '%s' (%zu bytes) eliminato, sostituito da '%s'", numGestoreConnessione, oldFile->path, oldFile->data_length, msg->path); fflush(stdout); ppff();
            
            setMessage(risposta, ANS_STREAM_FILE, 0, oldFile->path, oldFile->data, oldFile->data_length);
            checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio con file flushato");

            free(ht_remove(ht, oldFile->path));
            i++;
        } while((esitoControllo = checkLimits(msg, controllo)) != 0);

        setMessage(risposta, ANS_STREAM_END, 0, NULL, NULL, 0);
        checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio fine flush");
    }

    free(risposta);

    return i;
}


void printFiles() {
    ppf(CLR_WARNING); printf("------- LISTA ELEMENTI -------\n"); ppff();
    hash_elem_it it = HT_ITERATOR(ht);
    char* k = ht_iterate_keys(&it);
    while(k != NULL) {
        File* el = (File *)ht_get(ht, k);
        printSave("CHIAVE: %-10s | AUTORE: %-5d (aperto da %d) | DATA AGGIORNAMENTO: %-10ld | SPAZIO OCCUPATO: %-10ld", k, el->author, el->openBy, el->updatedDate, el->data_length);
        fflush(stdout);
        k = ht_iterate_keys(&it);
    }
    ppf(CLR_WARNING); printf("------ FINE LISTA ELEMENTI ------\n"); ppff();
}


static Message* elaboraAzione(int socketConnection, Message* msg, int numero) {
    // creo variabile per contenere richieste
    Message* risposta = cmalloc(sizeof(Message));
    checkStop(risposta == NULL, "malloc risposta");
    // fine variabile per contenere richieste

    setMessage(risposta, ANS_UNKNOWN, 0, NULL, NULL, 0);

    File* el = (File *)ht_get(ht, msg->path);

    switch(msg->action) {
        case REQ_OPEN: {
            switch(msg->flags) {
                case 0: { // file aperto e basta
                    if(el == NULL) {
                        setMessage(risposta, ANS_FILE_NOT_EXISTS, 0, NULL, NULL, 0);
                        ppf(CLR_ERROR); printSave("GC %d > Impossibile aprire file '%s' nella hash table: file inesistente", numero, msg->path); ppff();
                        break;
                    }

                    if(el->openBy != -1 && el->openBy != socketConnection) {
                        setMessage(risposta, ANS_NO_PERMISSION, 0, NULL, NULL, 0);
                        ppf(CLR_ERROR); printSave("GC %d > Impossibile aprire file '%s' nella hash table: è aperto da qualcun altro", numero, msg->path); ppff();
                        break;
                    }

                    // modifiche a file
                    el->openBy = socketConnection;
                    el->updatedDate = (unsigned long)time(NULL);
                    el->lastAction = REQ_OPEN;

                    // aggiorno le statistiche all'immissione di un file
                    locka(mutexStatistiche);
                    stats.n_openlock++;
                    if(stats.current_files_saved > stats.max_file_number_reached) stats.max_file_number_reached = stats.current_files_saved;
                    unlocka(mutexStatistiche);

                    setMessage(risposta, ANS_OK, 0, NULL, NULL, 0);
                    ppf(CLR_SUCCESS); printSave("GC %d > File '%s' aperto dalla hash table", numero, msg->path); ppff();
                    break;
                }

                case 1: { // file creato e aperto
                    if(el != NULL) {
                        setMessage(risposta, ANS_FILE_EXISTS, 0, NULL, NULL, 0);
                        ppf(CLR_ERROR); printSave("GC %d > Impossibile creare file '%s' nella hash table: esiste già", numero, msg->path); ppff();
                        break;
                    }

                    /* =========== CONTROLLO NUMERO FILE ================== */
                    expellFiles(msg, socketConnection, numero, 2);
                    /* =========== FINE CONTROLLO NUMERO FILE ================== */

                    // modifiche a file
                    File* f = cmalloc(sizeof(File));
                    f->openBy = socketConnection;
                    f->author = socketConnection;
                    f->updatedDate = (unsigned long)time(NULL);
                    f->lastAction = REQ_OPEN;
                    strcpy(f->path, msg->path);
                    checkStop(ht_put(ht, msg->path, f) == HT_ERROR, "creazione e apertura file senza lock");

                    // aggiornamento statistiche
                    locka(mutexStatistiche);
                    stats.n_openlock++;
                    stats.current_files_saved++;
                    if(stats.current_files_saved > stats.max_file_number_reached) stats.max_file_number_reached = stats.current_files_saved;
                    unlocka(mutexStatistiche);

                    setMessage(risposta, ANS_OK, 0, NULL, NULL, 0);
                    ppf(CLR_SUCCESS); printSave("GC %d > File '%s' inserito nella hash table", numero, msg->path); ppff();
                    break;
                }

                default: {
                    setMessage(risposta, ANS_UNKNOWN, 0, NULL, NULL, 0);
                    ppf(CLR_ERROR); printSave("GC %d > Flag impostata '%d' non valida o non supportata", msg->flags, numero); ppff();
                }
            }
            break;
        }

        case REQ_READ: {
            if(el == NULL) { // il file non esiste
                setMessage(risposta, ANS_FILE_NOT_EXISTS, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printSave("GC %d > File '%s' non trovato, impossibile leggerlo", numero, msg->path); ppff();
                break;
            }

            // modifiche a file
            el->updatedDate = (unsigned long)time(NULL);
            el->lastAction = REQ_READ;

            // aggiornamento statistiche
            locka(mutexStatistiche);
            stats.n_read++;
            stats.bytes_read += el->data_length;
            unlocka(mutexStatistiche);

            setMessage(risposta, ANS_OK, 0, msg->path, el->data, el->data_length);
            ppf(CLR_SUCCESS); printSave("GC %d > File '%s' trovato, invio", numero, msg->path); ppff();
            break;
        }

        case REQ_WRITE: {
            if(el == NULL) {
                setMessage(risposta, ANS_FILE_NOT_EXISTS, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printSave("GC %d > File '%s' non trovato, impossibile scriverci.", numero, msg->path); ppff();
                break;
            }

            if(el->lastAction != REQ_OPEN || el->data_length != 0) {
                setMessage(risposta, ANS_BAD_RQST, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printSave("GC %d > File '%s' non appena creato, impossibile scriverci.", numero, msg->path); ppff();
                break;
            }

            if(el->openBy != -1 && el->openBy != socketConnection) {
                setMessage(risposta, ANS_NO_PERMISSION, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printSave("GC %d > Impossibile scrivere sul file '%s' nella hash table perché è aperto da qualcun altro", numero, msg->path); ppff();
                break;
            }

            /* =========== CONTROLLO SFORAMENTO MEMORIA ================== */
            expellFiles(msg, socketConnection, numero, 1);
            /* =========== FINE CONTROLLO SFORAMENTO MEMORIA ============= */
            
            // modifiche a file
            el->data = msg->data;
            el->data_length = msg->data_length;
            el->updatedDate = (unsigned long)time(NULL);
            el->lastAction = REQ_WRITE;

            // aggiornamento statistiche
            locka(mutexStatistiche);
            stats.current_bytes_used += msg->data_length;
            if(stats.current_bytes_used > stats.max_size_reached) stats.max_size_reached = stats.current_bytes_used;
            stats.n_write++;
            stats.bytes_written += msg->data_length;
            unlocka(mutexStatistiche);

            setMessage(risposta, ANS_OK, 0, NULL, NULL, 0);
            ppf(CLR_SUCCESS); printSave("GC %d > File '%s' scritto nella hashtable", numero, msg->path); ppff();
            break;
        }

        case REQ_READ_N: {
            if(ht->e_num == 0) {
                setMessage(risposta, ANS_FILE_NOT_EXISTS, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printSave("GC %d > Non ci sono file da mandare, invio messaggio di errore.", numero); ppff();
                break;
            }

            setMessage(risposta, ANS_STREAM_START, 0, NULL, NULL, 0);
            checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio di inizio invio file multipli");

            int i = 0, bytes_letti = 0;

            hash_elem_it it = HT_ITERATOR(ht);
            char* k = ht_iterate_keys(&it);
            File* e = (File *)ht_get(ht, k);

            // mando un insieme di file, solo quelli per i quali il client ha i permessi di accesso
            while(e != NULL && (i < *msg->data || *msg->data == -1)) {
                // modifiche a file
                e->updatedDate = (unsigned long)time(NULL);
                e->lastAction = REQ_READ_N;

                setMessage(risposta, ANS_STREAM_FILE, 0, e->path, e->data, e->data_length);
                checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio con file inviato (multiplo)");
                
                ppf(CLR_SUCCESS); printSave("GC %d > File '%s' inviato", numero, e->path); ppff();

                bytes_letti += e->data_length;
                i++;
                k = ht_iterate_keys(&it);
                e = (File *)ht_get(ht, k);
            }

            setMessage(risposta, ANS_STREAM_END, 0, NULL, NULL, 0);
            checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio fine file");
            free(msg->data);

            // aggiornamento statistiche
            locka(mutexStatistiche);
            stats.n_read += i;
            stats.bytes_read += bytes_letti;
            unlocka(mutexStatistiche);
            
            setMessage(risposta, ANS_OK, 0, NULL, NULL, 0);
            break;
        }

        case REQ_CLOSE: {
            if(el == NULL) {
                setMessage(risposta, ANS_FILE_NOT_EXISTS, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printSave("GC %d > File '%s' non trovato, impossibile chiuderlo.", numero, msg->path); ppff();
                break;
            }

            if(el->openBy != -1 && el->openBy != socketConnection) { // il client può chiudere i suoi file e file non aperti (non cambia niente)
                setMessage(risposta, ANS_NO_PERMISSION, 0, NULL, NULL, 0);
                ppf(CLR_WARNING); printSave("GC %d > Il file '%s' non appartiene al client che ne ha richiesto la chiusura", numero, msg->path); ppff();
                break;
            }

            // modifiche a file
            el->openBy = -1;
            el->updatedDate = (unsigned long)time(NULL);
            el->lastAction = REQ_CLOSE;

            // aggiornamento statistiche
            locka(mutexStatistiche);
            stats.n_close++;
            unlocka(mutexStatistiche);

            setMessage(risposta, ANS_OK, 0, NULL, NULL, 0);
            ppf(CLR_SUCCESS); printSave("GC %d > File '%s' chiuso", numero, msg->path); ppff();
            break;
        }

        case REQ_DELETE: {
            if(el == NULL) { // il file da eliminare non esiste
                setMessage(risposta, ANS_FILE_NOT_EXISTS, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printSave("GC %d > Il file '%s' non esiste, impossibile eliminarlo", numero, msg->path); ppff();
                break;
            }

            if(el->openBy != -1 && el->openBy != socketConnection) { // il client può eliminare file non aperti da nessuno o i file aperti da lui
                setMessage(risposta, ANS_NO_PERMISSION, 0, NULL, NULL, 0);
                ppf(CLR_WARNING); printSave("GC %d > Il file '%s' non appartiene al client che ne ha richiesto l'eliminazione", numero, msg->path); ppff();
                break;
            }

            // modifiche a file
            free(ht_remove(ht, msg->path));

            // aggiornamento statistiche
            locka(mutexStatistiche);
            stats.current_files_saved--;
            stats.current_bytes_used = stats.current_bytes_used - el->data_length;
            stats.n_replace_applied++;
            stats.n_delete++;
            unlocka(mutexStatistiche);

            setMessage(risposta, ANS_OK, 0, NULL, NULL, 0);
            ppf(CLR_SUCCESS); printSave("GC %d > File '%s' eliminato", numero, msg->path); ppff();
            break;
        }

        case REQ_APPEND: {
            if(el == NULL) {
                setMessage(risposta, ANS_FILE_NOT_EXISTS, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printSave("GC %d > File '%s' non trovato, impossibile scriverci.", numero, msg->path); ppff();
                break;
            }

            if(el->openBy != -1 && el->openBy != socketConnection) {
                setMessage(risposta, ANS_NO_PERMISSION, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printSave("GC %d > Impossibile scrivere sul file '%s' nella hash table perché è aperto da qualcun altro", numero, msg->path); ppff();
                break;
            }

            /* =========== CONTROLLO SFORAMENTO MEMORIA ================== */
            expellFiles(msg, socketConnection, numero, 1);
            /* =========== FINE CONTROLLO SFORAMENTO MEMORIA ============= */

            // modifiche a file
            void* dato = cmalloc(el->data_length+msg->data_length);
            memcpy(dato, el->data, el->data_length);
            memcpy((char*)dato+(el->data_length), msg->data, msg->data_length);
            el->data = dato;
            el->data_length = el->data_length+msg->data_length;
            el->updatedDate = (unsigned long)time(NULL);
            el->lastAction = REQ_APPEND;

            // aggiorno le statistiche all'immissione di un file
            locka(mutexStatistiche);
            stats.current_bytes_used = stats.current_bytes_used + msg->data_length;
            if(stats.current_bytes_used > stats.max_size_reached) stats.max_size_reached = stats.current_bytes_used;
            stats.n_write++;
            stats.bytes_written += msg->data_length;
            unlocka(mutexStatistiche);

            setMessage(risposta, ANS_OK, 0, NULL, NULL, 0);
            ppf(CLR_SUCCESS); printSave("GC %d > File '%s' aggiornato nella hashtable.", numero, msg->path); ppff();
            break;
        }

        default: {
            char* testo = "Mi dispiace, non so gestire questa richiesta.";
            setMessage(risposta, ANS_BAD_RQST, 0, NULL, testo, strlen(testo));
        }
    }

    #ifdef DEBUG_VERBOSEE
    ppf(CLR_WARNING); printSave("------- LISTA ELEMENTI -------"); ppff();
	hash_elem_it it2 = HT_ITERATOR(ht);
	char* k = ht_iterate_keys(&it2);
	while(k != NULL) {
        el = ht_get(ht, k);
        if(el == NULL) {
            break;
        } 
		printSave("CHIAVE: %s | AUTORE: %d (%d) | DATA AGGIORNAMENTO: %ld | SPAZIO OCCUPATO: %ld", k, el->author, el->lock, el->updatedDate, el->data_length);
		k = ht_iterate_keys(&it2);
	}
    ppf(CLR_WARNING); printSave("------- FINE LISTA ELEMENTI -------"); ppff();
    #endif

    return risposta;
}


void* gestoreConnessione(void* n) {
    int numero = *((int*)n);

    free(n);

    while(TRUE) {
        locka(mutexCodaClient);
        locka(mutexChiusura);
        while(coda_client->usedSlots == 0 || chiusuraDebole || chiusuraForte) {
            if(chiusuraDebole || chiusuraForte) {
                unlocka(mutexChiusura);
                unlocka(mutexCodaClient);
                return (void*)NULL;
            } else {
                unlocka(mutexChiusura);
            }
            
            printSave("GC %d > Aspetto una connessione da gestire...", numero);
            checkStop(pthread_cond_wait(&clientInAttesa, &mutexCodaClient) != 0, "attesa di un client nella cond");
        
            locka(mutexChiusura);
        }
        // da qui in poi ho trovato un client da servire, ne ottengo il socket dalla coda
        unlocka(mutexChiusura);

        int socketConnection = removeFromList(coda_client);
        unlocka(mutexCodaClient);
        checkStop(socketConnection == -1, "rimozione primo elemento dalla coda in attesa fallita");
        printSave("GC %d > Connessione ottenuta dalla coda.", numero);
        

        // === INIZIO A SERVIRE IL CLIENT ===
        // creo variabile per contenere richieste
        Message* msg = cmalloc(sizeof(Message));
        checkStop(msg == NULL, "malloc risposta");
        // fine variabile per contenere richieste
        setMessage(msg, ANS_UNKNOWN, 0, NULL, NULL, 0);

        // invio messaggio di benvenuto al client
        Message* benvenuto = cmalloc(sizeof(Message));
        checkStop(benvenuto == NULL, "malloc benvenuto");

        char* testo_msg = "Benvenuto, sono in attesa di tue richieste";
        setMessage(benvenuto, ANS_WELCOME, 0, NULL, testo_msg, strlen(testo_msg));

        checkStop(sendMessage(socketConnection, benvenuto) != 0, "errore messaggio iniziale fallito");
        ppf(CLR_INFO); printSave("GC %d > WELCOME inviato al client, attendo una risposta HELLO...", numero); ppff();

        
        // attendo la msg del client al mio benvenuto
        int esito = readMessage(socketConnection, msg);
        checkStop(esito != 0 || msg->action != ANS_HELLO, "risposta WELCOME iniziale")
        ppf(CLR_SUCCESS); printSave("GC %d > Il client ha risposto al WELCOME: %s. Inizio comunicazione.", numero, msg->data); ppff();

        free(msg->data); // libero solo data perché non ricevo nessun path
        
        // qui inizia la comunicazione dopo la conferma dei messaggi WELCOME ED HELLO
        while((esito = readMessage(socketConnection, msg)) == 0) {
            stats.workerRequests[numero]++;
            ppf(CLR_INFO); printSave("GC %d > Arrivata richiesta di tipo: %s", numero, getRequestName(msg->action)); ppff();

            // controllo se c'è un segnale di terminazione
            locka(mutexChiusura);
            if(chiusuraForte) {
                unlocka(mutexChiusura);
                checkStop(close(socketConnection) == -1, "chiusura connessione dopo segnale");

                free(msg); // libero memoria del messaggio
                free(benvenuto); // libero memoria del messaggio

                return (void*)NULL;
            }
            unlocka(mutexChiusura);

            Message* risposta = elaboraAzione(socketConnection, msg, numero);

            checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio");

            //free(msg->path); // FIXME

            free(risposta);
        }

        free(msg); // libero memoria del messaggio
        free(benvenuto); // libero memoria del messaggio

        locka(mutexStatistiche);
        stats.current_connections--;
        unlocka(mutexStatistiche);

        printSave("GC %d > Connessione chiusa!", numero);
    }
    return (void*) NULL; // non proprio necessario
}


void *gestorePool(void* socket) {
    int socket_fd = *((int*)socket);

    while(TRUE) {
        locka(mutexChiusura);
        if(chiusuraDebole || chiusuraForte) {
            unlocka(mutexChiusura);
            break;
        } else {
            unlocka(mutexChiusura);
        }

        printSave("GP   > Aspetto di ricevere una connessione...");
        int socketConnection = accept(socket_fd, NULL, NULL);
        checkStop(socketConnection == -1, "accept nuova connessione da client");
        printSave("GP   > Connessione accettata a un client.");

        // controllo se ho già raggiunto il limite massimo di connessioni contemporanee
        locka(mutexStatistiche);
        if(stats.current_connections == config.max_connections) {
            unlocka(mutexStatistiche);
            ppf(CLR_ERROR); printSave("GP   > Raggiuto il limite di connessioni contemporanee. Invio messaggio di errore.\n"); ppff();
        
            Message* msg = cmalloc(sizeof(Message));
            checkStop(msg == NULL, "malloc errore max connessioni");

            setMessage(msg, ANS_MAX_CONN_REACHED, 0, NULL, NULL, 0);

            msg->data_length = 0;
            msg->data = NULL;

            checkStop(sendMessage(socketConnection, msg) != 0, "messaggio connessioni max raggiunto");
            free(msg); // libero memoria del messaggio
            continue;
        }

        stats.current_connections++;
        if(stats.current_connections > stats.max_concurrent_connections)
            stats.max_concurrent_connections = stats.current_connections;
        unlocka(mutexStatistiche);

        locka(mutexCodaClient);
        checkStop(addToList(coda_client, socketConnection) == -1, "impossibile aggiungere alla coda dei client in attesa");
        unlocka(mutexCodaClient);
        printSave("GP   > Connessione del client messa in coda!");
        checkStop(pthread_cond_signal(&clientInAttesa) != 0, "segnale arrivatoClient");
    }
    return (void*) NULL; // non proprio necessario
}


void *attesaSegnali(void* statFile) {
    int segnale;
    sigset_t insieme;

    checkStop(sigemptyset(&insieme) == -1, "settaggio a 0 di tutte le flag di insieme");
    checkStop(sigaddset(&insieme, SIGINT) == -1, "settaggio a 1 della flag per SIGINT");
    checkStop(sigaddset(&insieme, SIGTERM) == -1, "settaggio a 1 della flag per SIGTERM"); // windows
    checkStop(sigaddset(&insieme, SIGQUIT) == -1, "settaggio a 1 della flag per SIGQUIT");
    checkStop(sigaddset(&insieme, SIGHUP) == -1, "settaggio a 1 della flag per SIGHUP");

    checkStop(sigaddset(&insieme, SIGUSR1) == -1, "settaggio a 1 della flag per SIGUSR1");
    checkStop(sigaddset(&insieme, SIGUSR2) == -1, "settaggio a 1 della flag per SIGUSR2");

    while(TRUE) {
        checkStop(sigwait(&insieme, &segnale) != 0, "in attesa di un segnale gestito");

        if(segnale == SIGINT || segnale == SIGQUIT || segnale == SIGTERM) {
            ppf(CLR_IMPORTANT); printSave("GS   > Segnale TERMINAZIONE IMMEDIATA ricevuto."); ppff();

            locka(mutexChiusura);
            chiusuraForte = TRUE;
            unlocka(mutexChiusura);
            checkNull(pthread_cond_broadcast(&clientInAttesa) != 0, "signal clientInAttesa chiusura forte");
            break;
        }

        if(segnale == SIGHUP) {
            ppf(CLR_IMPORTANT); printSave("GS   > Segnale TERMINAZIONE LENTA ricevuto."); ppff();
            locka(mutexChiusura);
            chiusuraDebole = TRUE;
            unlocka(mutexChiusura);
            checkNull(pthread_cond_broadcast(&clientInAttesa) != 0, "signal clientInAttesa chiusura debole");
            break;
        }

        if(segnale == SIGUSR1) {
            printFiles();
        }

        if(segnale == SIGUSR2) {
            ht_destroy(ht);

            // creo tabella hash (salva i file)
            ht = ht_create(config.max_files);
            checkStop(ht == NULL, "creazione tabella hash");
            ppf(CLR_HIGHLIGHT); printf("GS   > Tabella ripulita!\n"); ppff();
            ppf(CLR_IMPORTANT); printf("GS   > Client connessi attualmente: %d\n", stats.current_connections); ppff();
            fflush(stdout);
        }
    }

    return (void*) NULL; // non proprio necessario
}


int main(int argc, char* argv[]) {
    int i;

    // ---- MASCHERO SEGNALI
    sigset_t insieme;
    checkStop(sigfillset(&insieme) == -1, "inizializzazione set segnali");
    checkStop(pthread_sigmask(SIG_SETMASK, &insieme, NULL) != 0, "mascheramento iniziale segnali");
    // gestisco in modo personalizzato i segnali
    struct sigaction ignoro;
    memset(&ignoro, 0, sizeof(ignoro));
    ignoro.sa_handler = SIG_IGN;
    checkStop(sigaction(SIGPIPE, &ignoro, NULL) == -1, "SC per ignorare segnale SIGPIPE");
    checkStop(sigaction(SIGSEGV, &ignoro, NULL) == -1, "SC per ignorare segnale SIGSEGV");
    // tolgo la maschera inserita all'inizio, la lascio solo per i segnali da gestire con thread
    checkStop(sigemptyset(&insieme) == -1, "settaggio a 0 di tutte le flag di insieme");
    checkStop(sigaddset(&insieme, SIGINT) == -1, "settaggio a 1 della flag per SIGINT");
    checkStop(sigaddset(&insieme, SIGTERM) == -1, "settaggio a 1 della flag per SIGTERM"); // windows
    checkStop(sigaddset(&insieme, SIGQUIT) == -1, "settaggio a 1 della flag per SIGQUIT");
    checkStop(sigaddset(&insieme, SIGHUP) == -1, "settaggio a 1 della flag per SIGHUP");
    checkStop(sigaddset(&insieme, SIGUSR1) == -1, "settaggio a 1 della flag per SIGUSR1");
    checkStop(sigaddset(&insieme, SIGUSR2) == -1, "settaggio a 1 della flag per SIGUSR2");
    checkStop(pthread_sigmask(SIG_SETMASK, &insieme, NULL) != 0, "rimozione mascheramento iniziale dei segnali ignorati o gestiti in modo custom");

    // LETTURA POSIZIONE FILE DI CONFIG
    char* posConfig = NULL;

    if(argc >= 3 && strcmp(argv[1], "-conf") == 0) posConfig = argv[2];

    if(posConfig == NULL) {
        posConfig = "TestDirectory/serverconfigs/config_default.txt";
        ppf(CLR_WARNING); printf("> Non hai specificato una posizione del file di config. Posizione config default: TestDirectory/serverconfigs\n"); ppff();
    }

    configLoader(&config, posConfig);

    // INIZIALIZZO FILE LOG
    fileLog = fopen(config.log_file_name, "w+");
    checkStop(fileLog == NULL, "creazione file log");


    time_t tempo;
    char buffer[80];
    time(&tempo);
    strftime(buffer, 80, "%c", localtime(&tempo));

    ppf(CLR_INFO); printSave("----- INIZIO SERVER [Data: %s] -----", buffer); ppff();
    

    // crea coda connessione
    checkStop((coda_client = createList(config.max_connections)) == NULL, "creazione coda connessioni");

    // creo tabella hash (salva i file)
    ht = ht_create(config.max_files);
    checkStop(ht == NULL, "creazione tabella hash");

    nmutex = config.max_files;
    mutexHT = cmalloc(nmutex*sizeof(pthread_mutex_t));
    checkStop(mutexHT == NULL, "creazione mutexHT iniziale");
    for(i = 0; i < nmutex; i++) pthread_mutex_init(&mutexHT[i], NULL);

    
    /*========= CONNESSIONE =========*/
    unlink(config.socket_file_name); // FIXME da rimuovere quando sta roba sarà più stabile
    // 1. SOCKET
    int socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    checkStop(socket_fd == -1, "creazione socket iniziale");

    // 2. BIND
    struct sockaddr_un address;
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, config.socket_file_name, strlen(config.socket_file_name)+1);
    checkStop(bind(socket_fd, (struct sockaddr*)&address, sizeof(address)) == -1, "bind del socket");

    // 3. LISTEN
    checkStop(listen(socket_fd, SOMAXCONN) == -1, "listen del socket"); // limite massimo possibile di connessioni

    ppf(CLR_SUCCESS); printSave("MAIN > Connessione pronta!"); ppff();

    /*========= AVVIO THREAD =========*/
    pthread_t idGestore;

    // gestore pool (accetta le connessioni e spedisce ai gestoreConnessione)
    checkStop(pthread_create(&idGestore, NULL, gestorePool, (void*)&socket_fd) != 0, "creazione gestorePool");
    printSave("MAIN > Thread gestore connessioni generato.");

    // creo l'insieme di workers
    pthread_t* idPool = cmalloc(config.max_workers * sizeof(pthread_t));
    checkStop(idPool == NULL, "malloc id pool di thread");

    stats.workerRequests = (int*)malloc(config.max_workers*sizeof(int));
    int *copyWR = stats.workerRequests;
    for(i = 0; i < config.max_workers; i++) {
        stats.workerRequests[i] = 0;
    }

    // faccio create per ogni worker specificato nel config
    for(i = 0; i < config.max_workers; i++) {
        int *argomento = cmalloc(sizeof(*argomento));
        checkStop(argomento == NULL, "allocazione memoria argomento thread");

        *argomento = i;

        checkStop(pthread_create(&idPool[i], NULL, gestoreConnessione, argomento) != 0, "creazione gestoreConnessione nella pool");
    }

    pthread_t sigID;
    // creo il thread che gestisce i segnali
	checkStop(pthread_create(&sigID, NULL, attesaSegnali, (void*)NULL) != 0, "creazione thread per attesa segnali");
	printSave("MAIN > Thread gestore segnali generato.");


    /*========= TERMINE THREAD =========*/
    
	// in attesa della terminazione del thread che gestisce i segnali
	checkStop(pthread_join(sigID, NULL) != 0, "join del thread per attesa segnali");
	printSave("MAIN > Thread gestore segnali terminato.");
    
    // in attesa della terminazione dei workers nella pool
    for(i = 0; i < config.max_workers; i++) {
        checkStop(pthread_join(idPool[i], NULL) != 0, "join di un thread nella pool");
    }
    free(idPool);

    // elimino il thread
    int returnval = pthread_cancel(idGestore);
    checkStop(returnval != 0 && returnval != 3, "cancellazione thread gestore");

    // in attesa della terminazione del thread gestore
    checkStop(pthread_join(idGestore, NULL) != 0, "join del gestore");
    printSave("MAIN > Thread gestore connessioni terminato.");


    // eliminazione socket, creazione statistiche, chiusura file log, liberazione memoria
    unlink(config.socket_file_name);
    deleteList(coda_client);

	printStats(config.stats_file_name, config.max_workers);
    printSave("MAIN > Le statistiche sono state salvate nel file '%s'.", config.stats_file_name);

    time(&tempo);
    strftime(buffer, 80, "%c", localtime(&tempo));
    ppf(CLR_INFO); printSave("----- FINE SERVER [Data: %s] -----", buffer); ppff();

    free(copyWR);
    free(mutexHT);
    ht_destroy(ht);
    fclose(fileLog);
    return 0;
}