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

#include "utils/includes/config.c"
#include "utils/includes/message.c"
#include "utils/includes/hash_table.c"
#include "communication.c"
#include "utils/includes/client_queue.c"
#include "utils/includes/statistics.c"

Config config;

Statistics stats = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
pthread_mutex_t mutexStatistiche = PTHREAD_MUTEX_INITIALIZER;

hashtable_t *ht = NULL;
pthread_mutex_t* mutexHT = NULL;
int nmutex = 0;

pthread_mutex_t mutexChiusura = PTHREAD_MUTEX_INITIALIZER;

ClientQueue *coda_client = NULL;
pthread_mutex_t mutexCodaClient = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t clientInAttesa = PTHREAD_COND_INITIALIZER;

int chiusuraForte  = FALSE;
int chiusuraDebole = FALSE;

static int checkLimits(Message* msg, int which) {
    if(stats.current_bytes_used + msg->data_length > config.max_memory_size && (which == 1 || which == 3)) { // l'inserimento del file causerebbe sforamento limiti memoria
        return -1;
    } else if(stats.current_files_saved + 1 > config.max_files && (which == 2 || which == 3)) { // l'inserimento del file causerebbe sforamento limiti quantità file
        return -2;
    } else { // l'inserimento del file non causerebbe problemi
        return 0;
    }
}

static Message* elaboraAzione(int socketConnection, Message* msg) {
    // creo variabile per contenere richieste
    Message* risposta = calloc(4, sizeof(Message));
    checkStop(risposta == NULL, "malloc risposta");
    // fine variabile per contenere richieste

    setMessage(risposta, AC_UNKNOWN, 0, NULL, NULL, 0);

    hash_elem_t* el = ht_getElement(ht, msg->path);

    switch(msg->action) {
        case AC_OPEN: { // O_CREATE = 1 | O_LOCK = 2 | O_CREATE && O_LOCK = 3
            if(msg->path == NULL || msg->path_length == 0) { // controllo che il path non sia vuoto
                setMessage(risposta, AC_BADRQST, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printf("GC|EA> Il client %d ha mandato una OPEN con path nullo.\n", socketConnection); ppff();
                break;
            }

            /* =========== CONTROLLO CHE LE FLAG SIANO UTILIZZATE CORRETTAMENTE ================== */
            if(el != NULL && (msg->flags == 1 || msg->flags == 3)) { // viene fatta una create con un path che esiste già
                setMessage(risposta, AC_FILEEXISTS, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printf("GC|EA> Impossibile aggiungere file '%s' nella hash table: esiste già\n", msg->path); ppff();
            } else if (msg->flags == 2 && el == NULL) { // viene fatta la lock su un file che non esiste ancora
                setMessage(risposta, AC_FILENOTEXISTS, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printf("GC|EA> Impossibile aggiungere file '%s' nella hash table: lock su file che non esiste\n", msg->path); ppff();
            } else {
                /* =========== CONTROLLO NUMERO FILE ================== */
                if(msg->flags != 2) { // se richiesta è solo una open con lock allora non c'è bisogno di controllare se sforo il limite
                    int controllo = checkLimits(msg, 2), pulizia = FALSE;
                    
                    if(controllo != 0) {
                        ppf(CLR_HIGHLIGHT); printf("GC|EA> Controllo limitazioni: NUMERO FILE\n"); fflush(stdout);
                        setMessage(risposta, AC_FLUSH_START, 0, NULL, NULL, 0);
                        checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio inizio flush");
                    }

                    while(controllo != 0 && ht->e_num != 0) { // devo vedere se il numero di file non è 0 altrimenti non riesco a trovare un valore vecchio sotto ed esplodo
                        pulizia = TRUE;
                        hash_elem_t* e = ht_find_old_entry(ht);

                        ppf(CLR_ERROR); printf("GC|EA> File '%s' (%zu bytes) eliminato per inserire '%s' nella HT: avrei raggiunto il limite di %d file\n", e->path, e->data_length, msg->path, config.max_files); fflush(stdout); ppff();
                        locka(mutexStatistiche);
                        stats.current_files_saved--;
                        unlocka(mutexStatistiche);

                        setMessage(risposta, AC_FLUSHEDFILE, 0, e->path, e->data, e->data_length);
                        checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio con file flushato");

                        ht_remove(ht, e->path);
                        free(e);

                        // rieseguo controllo
                        controllo = checkLimits(msg, 2);
                    }

                    if(pulizia) {
                        setMessage(risposta, AC_FLUSH_END, 0, NULL, NULL, 0);
                        checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio fine flush");
                    }
                }
                /* =========== FINE CONTROLLO NUMERO FILE ================== */

                /* =========== CREO O APRO FILE (CON LOCK O SENZA LOCK) ================== */
                el = ht_getElement(ht, msg->path);
                
                switch(msg->flags) {
                    case 1: { // file creato e aperto (no lock)
                        checkStop(ht_put(ht, msg->path, NULL, 0, -1, (unsigned long)time(NULL)) == HT_ERROR, "creazione e apertura file senza lock");
                        ppf(CLR_SUCCESS); printf("GC|EA> File '%s' inserito nella hash table.\n", msg->path); ppff();
                        break;
                    }

                    case 2: { // file aperto (lock)
                        if(el != NULL) {
                            el->lock = socketConnection;
                        } else {
                            stampaDebug("GC|EA> Il file precedente è stato cancellato, lo rigenero e lo apro in lock!\n");
                            checkStop(ht_put(ht, msg->path, msg->data, msg->data_length, socketConnection, (unsigned long)time(NULL)) == HT_ERROR, "creazione e apertura file senza lock");
                        }
                        
                        ppf(CLR_SUCCESS); printf("GC|EA> File '%s' aperto e bloccato nella hash table.\n", msg->path); ppff();
                        break;
                    }

                    case 3: { // file creato e aperto (lock)
                        checkStop(ht_put(ht, msg->path, NULL, 0, socketConnection, (unsigned long)time(NULL)) == HT_ERROR, "creazione e apertura file con lock");
                        ppf(CLR_SUCCESS); printf("GC|EA> File '%s' creato e aperto nella hash table.\n", msg->path); ppff();
                        break;
                    }

                    default: {
                        setMessage(risposta, AC_UNKNOWN, 0, NULL, NULL, 0);
                        ppf(CLR_ERROR); printf("GC|EA> Flag non valida!!\n"); ppff();
                    }
                }
                setMessage(risposta, AC_FILEOPENED, 0, NULL, NULL, 0);

                // aggiorno le statistiche all'immissione di un file
                locka(mutexStatistiche);
                // aggiorno i file salvati
                stats.current_files_saved++;
                if(stats.current_files_saved > stats.max_file_number_reached) {
                    stats.max_file_number_reached = stats.current_files_saved;
                }
                // aggiorno i byte usati
                stats.current_bytes_used = stats.current_bytes_used + msg->data_length;
                if(stats.current_bytes_used > stats.max_size_reached) {
                    stats.max_size_reached = stats.current_bytes_used;
                }
                unlocka(mutexStatistiche);
            }
            break;
        }

        case AC_READ: {
            if(el != NULL) { // il file esiste
                if(el->lock == -1 || el->lock == socketConnection) {
                    ppf(CLR_SUCCESS); printf("GC|EA> File '%s' trovato!\n", msg->path); ppff();
                    setMessage(risposta, AC_FILESVD, 0, msg->path, el->data, el->data_length);
                } else {
                    ppf(CLR_WARNING); printf("GC|EA> File '%s' trovato, ma il client %d non ne possiede i diritti di lettura/scrittura.\n", msg->path, socketConnection); ppff();
                    setMessage(risposta, AC_NOTFORU, 0, NULL, NULL, 0);
                }
            } else {
                ppf(CLR_ERROR); printf("GC|EA> File '%s' non trovato, invio messaggio di errore.\n", msg->path); ppff();
                setMessage(risposta, AC_FILENOTEXISTS, 0, NULL, NULL, 0);
            }
            break;
        }

        
        case AC_WRITE: {
            if((el->data_length == 0 && el->data == NULL) && el->lock == socketConnection) { // se il file è stato appena creato ed il client corrente ne è l'owner
                /* =========== CONTROLLO LIMITE MEMORIA ================== */
                int controllo = checkLimits(msg, 1), pulizia = FALSE;
                
                if(controllo != 0) {
                    ppf(CLR_HIGHLIGHT); printf("GC|EA> Controllo limitazioni: "); if(controllo == -1) { printf("sforamento memoria.\n"); } else if(controllo == -2) {  printf("sforamento limite files.\n"); } fflush(stdout);
                    setMessage(risposta, AC_FLUSH_START, 0, NULL, NULL, 0);
                    checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio inizio flush");
                }

                while(controllo != 0 && ht->e_num != 0) { // devo vedere se il numero di file non è 0 altrimenti non riesco a trovare un valore vecchio sotto ed esplodo
                    pulizia = TRUE;
                    hash_elem_t* e = ht_find_old_entry(ht);

                    ppf(CLR_ERROR); printf("GC|EA> File '%s' (%zu bytes) eliminato per scrivere '%s' nella HT: avrei sforato (%d bytes) il limite di %d bytes\n", e->path, e->data_length, msg->path, (msg->data_length+stats.current_bytes_used), config.max_memory_size); fflush(stdout); ppff();
                    locka(mutexStatistiche);
                    stats.current_bytes_used = stats.current_bytes_used - e->data_length;
                    unlocka(mutexStatistiche);

                    setMessage(risposta, AC_FLUSHEDFILE, 0, e->path, e->data, e->data_length);
                    checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio con file flushato");

                    ht_remove(ht, e->path);
                    free(e);

                    // rieseguo controllo
                    controllo = checkLimits(msg, 1);
                }

                if(pulizia) {
                    setMessage(risposta, AC_FLUSH_END, 0, NULL, NULL, 0);
                    checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio fine flush");
                }
                /* =========== FINE CONTROLLO MEMORIA ================== */

                el->data = msg->data;
                el->data_length = msg->data_length;
                setMessage(risposta, AC_FILERCVD, 0, NULL, NULL, 0);
                ppf(CLR_SUCCESS); printf("GC|EA> File '%s' scritto nella hashtable.\n", msg->path); ppff();
            } else {
                setMessage(risposta, AC_FILENOTNEW, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printf("GC|EA> File '%s' non appena creato, impossibile scriverci.\n", msg->path); ppff();
            }

            break;
        }

        case AC_READ_MULTIPLE: {
            int i = 0;

            if(ht->e_num == 0) {
                setMessage(risposta, AC_FILENOTEXISTS, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printf("GC|EA> Non ci sono file da mandare. Invio messaggio di errore.\n"); ppff();
            } else {
                setMessage(risposta, AC_START_SEND, 0, NULL, NULL, 0);
                checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio di inizio invio file multipli");

                hash_elem_it it = HT_ITERATOR(ht);
                hash_elem_t* e = ht_iterate(&it);
                // mando un insieme di file, solo quelli per i quali il client ha i permessi di accesso
                while(e != NULL && (i < *msg->data || *msg->data == -1) && (e->lock == -1 || e->lock == socketConnection)) {
                    setMessage(risposta, AC_FILE_SENT, 0, e->path, e->data, e->data_length);
                    checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio con file inviato (multiplo)");
                    ppf(CLR_SUCCESS); printf("GC|EA> File '%s' inviato.\n", e->path); ppff();

                    e = ht_iterate(&it);
                    i++;
                }

                setMessage(risposta, AC_FINISH_SEND, 0, NULL, NULL, 0);
            }

            break;
        }

        case AC_CLOSE: {
            hash_elem_t* valore = ht_getElement(ht, msg->path);

            if(valore->lock == -1 || valore->lock == socketConnection) { // puoi chiudere file solo se non c'ha lock oppure se c'è ed è fatto dallo stesso client
                valore->lock = -1;
                setMessage(risposta, AC_CLOSED, 0, NULL, NULL, 0);
                ppf(CLR_SUCCESS); printf("GC|EA> File '%s' chiuso.\n", msg->path); ppff();
            } else {
                setMessage(risposta, AC_NOTFORU, 0, NULL, NULL, 0);
                ppf(CLR_WARNING); printf("GC|EA> Il file '%s' non appartiene al client che ne ha richiesto la chiusura.\n", msg->path); ppff();
            }

            break;
        }

        case AC_DELETE: {
            hash_elem_t* valore = ht_getElement(ht, msg->path);
            
            if(valore == NULL) {
                setMessage(risposta, AC_FILENOTEXISTS, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printf("GC|EA> Il file '%s' non esiste. Impossibile eliminarlo.\n", msg->path); ppff();
            } else {
                if(valore->lock == -1 || valore->lock == socketConnection) { // puoi cancellare file solo se non c'ha lock oppure se c'è ed è fatto dallo stesso client
                    ht_remove(ht, msg->path);
                    setMessage(risposta, AC_DELETED, 0, NULL, NULL, 0);
                    ppf(CLR_SUCCESS); printf("GC|EA> File '%s' eliminato.\n", msg->path); ppff();
                } else {
                    setMessage(risposta, AC_NOTFORU, 0, NULL, NULL, 0);
                    ppf(CLR_WARNING); printf("GC|EA> Il file '%s' non appartiene al client che ne ha richiesto l'eliminazione.\n", msg->path); ppff();
                }
            }
            break;
        }

        case AC_APPEND: { // TODO ancora non funzionante (non viene fatta l'append)
            if(el->lock == socketConnection) { // se il file non è lockato sull'owner attuale
                /* =========== CONTROLLO MEMORIA ================== */
                int controllo = checkLimits(msg, 1), pulizia = FALSE;
                
                if(controllo != 0) {
                    ppf(CLR_HIGHLIGHT); printf("GC|EA> Controllo limitazioni: sforamento memoria\n"); fflush(stdout);
                    setMessage(risposta, AC_FLUSH_START, 0, NULL, NULL, 0);
                    checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio inizio flush");
                }

                while(controllo != 0 && ht->e_num != 0) { // devo vedere se il numero di file non è 0 altrimenti non riesco a trovare un valore vecchio sotto ed esplodo
                    pulizia = TRUE;
                    hash_elem_t* e = ht_find_old_entry(ht);

                    locka(mutexStatistiche);
                    if(controllo == -1) {
                        ppf(CLR_ERROR); printf("GC|EA> File '%s' (%zu bytes) eliminato per fare la append su '%s' nella HT: avrei sforato (%d bytes) il limite di %d bytes\n", e->path, e->data_length, msg->path, (msg->data_length+stats.current_bytes_used), config.max_memory_size); fflush(stdout); ppff();
                        stats.current_bytes_used = stats.current_bytes_used - e->data_length;
                    }
                    unlocka(mutexStatistiche);

                    setMessage(risposta, AC_FLUSHEDFILE, 0, e->path, e->data, e->data_length);
                    checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio con file flushato");

                    ht_remove(ht, e->path);
                    free(e);

                    // rieseguo controllo
                    controllo = checkLimits(msg, 1);
                }

                if(pulizia) {
                    setMessage(risposta, AC_FLUSH_END, 0, NULL, NULL, 0);
                    checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio fine flush");
                }
                /* =========== FINE CONTROLLO MEMORIA ================== */

                void* dato = malloc(el->data_length+msg->data_length);
                memset(dato, 0, el->data_length+msg->data_length);
                memcpy(dato, el->data, el->data_length);
                memcpy((char*)dato+(el->data_length), msg->data, msg->data_length);

                el->data = dato;
                el->data_length = el->data_length+msg->data_length;
                setMessage(risposta, AC_FILESVD, 0, NULL, NULL, 0);
                ppf(CLR_SUCCESS); printf("GC|EA> File '%s' aggiornato nella hashtable.\n", msg->path); ppff();
            } else {
                setMessage(risposta, AC_NOTFORU, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printf("GC|EA> File '%s' non appena creato, impossibile scriverci.\n", msg->path); ppff();
            }
            break;
        }

        case AC_LOCK: {
            if(el != NULL) {
                if(el->lock == -1 || el->lock == socketConnection) { // il lock non c'è o è già settato dal client che ha fatto richiesta
                    el->lock = socketConnection; // ottengo il lock
                    setMessage(risposta, AC_LOCKED, 0, NULL, NULL, 0);
                    ppf(CLR_SUCCESS); printf("GC|EA> File '%s' lockato dal client %d.\n", msg->path, socketConnection); ppff();
                } else { // il lock è di qualcun altro
                    if(el->codaRichiedentiLock == NULL) {
                        el->codaRichiedentiLock = createQueue(config.max_workers);
                        checkStop(el->codaRichiedentiLock == NULL, "creazione coda richiedenti lock AC_LOCK");
                    }

                    checkStop(addToQueue(el->codaRichiedentiLock, socketConnection) == -1, "aggiunta client coda richiedenti lock AC_LOCK");
                
                    if(el->rilascioLock == NULL) {
                        el->rilascioLock = malloc(sizeof(pthread_cond_t));
                        checkStop(el->rilascioLock == NULL, "creazione cond rilasciolock AC_LOCK");
                        checkStop(pthread_cond_init(el->rilascioLock, NULL), "inizializzazione cond rilasciolock AC_LOCK");
                    }

                    stampaDebug("GC|EA> In attesa di acquisire la lock dell'oggetto!");

                    while(TRUE) {
                        checkStop(pthread_cond_wait(el->rilascioLock, el->mutex) != 0, "attesa rilascio lock oggetto");
                        stampaDebug("GC|EA> Svegliato dall'attesa per la lock!");

                        el = ht_getElement(ht, msg->path);

                        if(el == NULL) { // quel file non esiste più
                            setMessage(risposta, AC_FILENOTEXISTS, 0, NULL, NULL, 0);
                            break;
                        }

                        if(el->lock == socketConnection) { // ho ottenuto il lock
                            setMessage(risposta, AC_LOCKED, 0, NULL, NULL, 0);
                            break;
                        }
                    }
                    stampaDebug("GC|EA> Uscito dal while attesa lock");

                    if(risposta->action == AC_FILENOTEXISTS) {
                        stampaDebug("GC|EA> Il file è stato cancellato mentre attendevo la lock!");
                    } else {
                        stampaDebug("GC|EA> Sono uscito dal while attesa lock e ho ottenuto il lock.");
                        if(el->codaRichiedentiLock == NULL) {
                            free(el->rilascioLock);
                            el->rilascioLock = NULL;
                        }
                    }
                }
            } else {
                setMessage(risposta, AC_FILENOTEXISTS, 0, NULL, NULL, 0);
            }

            break;
        }

        case AC_UNLOCK: {
            if(el != NULL) {
                if(el->lock == socketConnection) {
                    if(el->codaRichiedentiLock == NULL) {
                        el->lock = -1;
                        stampaDebug("GC|EA> Lock rimossa perché non ci sono client in attesa.");
                    } else {
                        el->lock = removeFromQueue(el->codaRichiedentiLock);

                        stampaDebug("GC|EA> Lock assegnata al primo in coda.");
                        if(el->codaRichiedentiLock->numClients == 0) {
                            deleteQueue(el->codaRichiedentiLock);
                            el->codaRichiedentiLock = NULL;
                            stampaDebug("GC|EA> Ho eliminato la coda visto che il client estratto era l'ultimo");
                        }

                        if(el->rilascioLock != NULL) {
                            if(pthread_cond_broadcast(el->rilascioLock) == 0) {
                                ppf(CLR_ERROR); printf("GC|EA> Errore broadcast lock rilasciato!!\n"); ppff();
                            }
                        } else {
                            ppf(CLR_ERROR); printf("GC|EA> Variabile di condizione lock inesistente!!\n"); ppff();
                        }
                    }
                    
                    setMessage(risposta, AC_UNLOCKED, 0, NULL, NULL, 0);
                } else {
                    setMessage(risposta, AC_NOTFORU, 0, NULL, NULL, 0);
                }
            } else {
                setMessage(risposta, AC_FILENOTEXISTS, 0, NULL, NULL, 0);
            }

            break;
        }

        case AC_UNKNOWN: {
            char* testo = "Hai inviato una richiesta non valida.";
            setMessage(risposta, AC_CANTDO, 0, NULL, testo, strlen(testo));
            break;
        }

        default: {
            char* testo = "Mi dispiace, ma non so gestire questo tipo di richieste ora.";
            setMessage(risposta, AC_CANTDO, 0, NULL, testo, strlen(testo));
        }
    }

    printf("---- LISTA ELEMENTI ----\n");
    fflush(stdout);
	hash_elem_it it2 = HT_ITERATOR(ht);
	char* k = ht_iterate_keys(&it2);
	while(k != NULL) {
        el = ht_getElement(ht, k);
		printf("CHIAVE %s | PATH: %s | LOCK: %d | DATA AGGIORNAMENTO: %ld\n", k, el->path, el->lock, el->updatedDate);
        fflush(stdout);
		k = ht_iterate_keys(&it2);
	}

    return risposta;
}

void* gestoreConnessione() {
    while(TRUE) {
        locka(mutexCodaClient);
        locka(mutexChiusura);
        while(coda_client->numClients == 0 || chiusuraDebole || chiusuraForte) {
            if(chiusuraDebole || chiusuraForte) {
                unlocka(mutexChiusura);
                unlocka(mutexCodaClient);
                return (void*)NULL;
            } else {
                unlocka(mutexChiusura);
            }
            
            stampaDebug("GC> Aspetto una connessione da gestire...");
            checkStop(pthread_cond_wait(&clientInAttesa, &mutexCodaClient) != 0, "attesa di un client nella cond");
        
            locka(mutexChiusura);
        }
        // da qui in poi ho trovato un client da servire, ne ottengo il socket dalla coda
        unlocka(mutexChiusura);

        int socketConnection = removeFromQueue(coda_client);
        unlocka(mutexCodaClient);
        checkStop(socketConnection == -1, "rimozione primo elemento dalla coda in attesa fallita");
        stampaDebug("GC> Connessione ottenuta dalla coda.");
        

        // === INIZIO A SERVIRE IL CLIENT ===
        // creo variabile per contenere richieste
        Message* msg = calloc(4, sizeof(Message));
        checkStop(msg == NULL, "malloc risposta");
        // fine variabile per contenere richieste
        setMessage(msg, AC_UNKNOWN, 0, NULL, NULL, 0);

        // invio messaggio di benvenuto al client
        Message* benvenuto = calloc(4, sizeof(Message));
        checkStop(benvenuto == NULL, "malloc benvenuto");

        char* testo_msg = "Benvenuto, sono in attesa di tue richieste";
        setMessage(benvenuto, AC_WELCOME, 0, NULL, testo_msg, strlen(testo_msg));

        checkStop(sendMessage(socketConnection, benvenuto) != 0, "errore messaggio iniziale fallito");
        ppf(CLR_INFO); printf("GC> WELCOME inviato al client, attendo una risposta HELLO...\n"); ppff();

        
        // attendo la msg del client al mio benvenuto
        int esito = readMessage(socketConnection, msg);
        checkStop(esito != 0 || msg->action != AC_HELLO, "risposta WELCOME iniziale")
        ppf(CLR_SUCCESS); fprintf(stdout, "GC> Il client ha risposto al WELCOME: %s. Inizio comunicazione.\n", msg->data); ppff();

        free(msg->path);
        free(msg->data);
        
        // qui inizia la comunicazione dopo la conferma dei messaggi WELCOME ED HELLO
        esito = readMessage(socketConnection, msg);
        while(esito == 0) {
            ppf(CLR_INFO); printf("GC> Arrivata richiesta di tipo: %d!\n", msg->action); ppff();

            // controllo se c'è un segnale di terminazione
            locka(mutexChiusura);
            if(chiusuraForte) {
                unlocka(mutexChiusura);
                setMessage(msg, AC_STOPPING, 0, NULL, NULL, 0);
                checkStop(sendMessage(socketConnection, msg) != 0, "errore messaggio iniziale fallito");
                checkStop(close(socketConnection) == -1, "chiusura connessione dopo segnale");

                free(msg->path);
                free(msg->data);
                free(msg); // libero memoria del messaggio
                free(benvenuto); // libero memoria del messaggio

                return (void*)NULL;
            }
            unlocka(mutexChiusura);

            Message* risposta = elaboraAzione(socketConnection, msg);

            checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio");

        
            free(risposta);

            // richiesta dopo
            esito = readMessage(socketConnection, msg);
        }

        free(msg->path);
        free(msg->data);
        free(msg); // libero memoria del messaggio
        free(benvenuto); // libero memoria del messaggio

        locka(mutexStatistiche);
        stats.current_connections--;
        unlocka(mutexStatistiche);

        // faccio una unlock nel caso il client si fosse scordato di farla
        hash_elem_it it = HT_ITERATOR(ht);
        char* k = ht_iterate_keys(&it);
        while(k != NULL) {
            hash_elem_t* el = ht_getElement(ht, k);
            if(el->lock == socketConnection) {
                el->lock = -1;
                ppf(CLR_WARNING); printf("GC> Il client n°%d ha lasciato una lock sul file '%s'. Glie l'ho tolto.\n", socketConnection, el->path); ppff();
            }

            k = ht_iterate_keys(&it);
        }

        

        stampaDebug("GC> Connessione chiusa!");
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

        stampaDebug("GP> Aspetto di ricevere una connessione...");
        int socketConnection = accept(socket_fd, NULL, NULL);
        checkStop(socketConnection == -1, "accept nuova connessione da client");
        stampaDebug("GP> Connessione accettata a un client.");

        // controllo se ho già raggiunto il limite massimo di connessioni contemporanee
        locka(mutexStatistiche);
        if(stats.current_connections == config.max_connections) {
            unlocka(mutexStatistiche);
            ppf(CLR_ERROR); printf("GP> Raggiuto il limite di connessioni contemporanee. Invio messaggio di errore.\n"); ppff();
        
            Message* msg = malloc(sizeof(Message));
            checkStop(msg == NULL, "malloc errore max connessioni");

            setMessage(msg, AC_MAXCONNECTIONSREACHED, 0, NULL, NULL, 0);

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
        checkStop(addToQueue(coda_client, socketConnection) == -1, "impossibile aggiungere alla coda dei client in attesa");
        unlocka(mutexCodaClient);
        stampaDebug("GP> Connessione del client messa in coda!");
        checkStop(pthread_cond_signal(&clientInAttesa) != 0, "segnale arrivatoClient");
    }
    return (void*) NULL; // non proprio necessario
}

void *attesaSegnali(void* statFile) {
    int segnale;
    sigset_t insieme;

    checkStop(sigemptyset(&insieme) == -1, "settaggio a 0 di tutte le flag di insieme");
    checkStop(sigaddset(&insieme, SIGINT) == -1, "settaggio a 1 della flag per SIGINT");
    checkStop(sigaddset(&insieme, SIGQUIT) == -1, "settaggio a 1 della flag per SIGTERM");
    checkStop(sigaddset(&insieme, SIGHUP) == -1, "settaggio a 1 della flag per SIGQUIT");

    checkStop(sigaddset(&insieme, SIGUSR1) == -1, "settaggio a 1 della flag per SIGUSR1");
    checkStop(sigaddset(&insieme, SIGUSR2) == -1, "settaggio a 1 della flag per SIGUSR2");

    while(TRUE) {
        checkStop(sigwait(&insieme, &segnale) != 0, "in attesa di un segnale gestito");

        if(segnale == SIGINT || segnale == SIGQUIT) {
            stampaDebug("GS> Segnale TERMINAZIONE IMMEDIATA ricevuto.");

            locka(mutexChiusura);
            chiusuraForte = TRUE;
            unlocka(mutexChiusura);
            checkNull(pthread_cond_broadcast(&clientInAttesa) != 0, "signal clientInAttesa")
            break;
        }

        if(segnale == SIGHUP) {
            stampaDebug("GS> Segnale TERMINAZIONE LENTA ricevuto.");
            locka(mutexChiusura);
            chiusuraDebole = TRUE;
            unlocka(mutexChiusura);
            checkNull(pthread_cond_broadcast(&clientInAttesa) != 0, "signal clientInAttesa")
            break;
        }

        if(segnale == SIGUSR1) {
            printf("------- LISTA ELEMENTI -------\n");
            fflush(stdout);
            hash_elem_it it2 = HT_ITERATOR(ht);
            char* k = ht_iterate_keys(&it2);
            while(k != NULL) {
                hash_elem_t* el = ht_getElement(ht, k);
                printf("CHIAVE %s | PATH: %s | LOCK: %d | DATA AGGIORNAMENTO: %ld\n", k, el->path, el->lock, el->updatedDate);
                fflush(stdout);
                k = ht_iterate_keys(&it2);
            }
            printf("------ FINE LISTA ELEMENTI ------\n");
        }

        if(segnale == SIGUSR2) {
            ht_clear(ht, 1);

            // creo tabella hash (salva i file)
            ht = ht_create(config.max_files);
            checkStop(ht == NULL, "creazione tabella hash");
        }
    }

    return (void*) NULL; // non proprio necessario
}

int main(int argc, char* argv[]) {
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
    checkStop(sigaddset(&insieme, SIGQUIT) == -1, "settaggio a 1 della flag per SIGQUIT");
    checkStop(sigaddset(&insieme, SIGHUP) == -1, "settaggio a 1 della flag per SIGHUP");
    checkStop(sigaddset(&insieme, SIGUSR1) == -1, "settaggio a 1 della flag per SIGUSR1");
    checkStop(sigaddset(&insieme, SIGUSR2) == -1, "settaggio a 1 della flag per SIGUSR2");
    checkStop(pthread_sigmask(SIG_SETMASK, &insieme, NULL) != 0, "rimozione mascheramento iniziale dei segnali ignorati o gestiti in modo custom");


    ppf(CLR_INFO); printf("---- INIZIO SERVER ----\n"); ppff();
    configLoader(&config);

    // crea coda connessione
    checkStop((coda_client = createQueue(config.max_connections)) == NULL, "creazione coda connessioni");

    // creo tabella hash (salva i file)
    ht = ht_create(config.max_files);
    checkStop(ht == NULL, "creazione tabella hash");

    nmutex = config.max_files;
    mutexHT = malloc(nmutex*sizeof(pthread_mutex_t));
    checkStop(mutexHT == NULL, "creazione mutexHT iniziale");
    for(int i = 0; i < nmutex; i++) pthread_mutex_init(&mutexHT[i], NULL);

    
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

    ppf(CLR_SUCCESS); printf("Connessione pronta!\n"); ppff();

    /*========= AVVIO THREAD =========*/
    pthread_t idGestore;

    // gestore pool (accetta le connessioni e spedisce ai gestoreConnessione)
    checkStop(pthread_create(&idGestore, NULL, gestorePool, (void*)&socket_fd) != 0, "creazione gestorePool");
    stampaDebug("> Thread gestore connessioni generato.");

    // creo l'insieme di workers
    pthread_t* idPool = malloc(config.max_workers * sizeof(pthread_t));
    checkStop(idPool == NULL, "malloc id pool di thread");

    // faccio create per ogni worker specificato nel config
    for(int i = 0; i < config.max_workers; i++) {
        checkStop(pthread_create(&idPool[i], NULL, gestoreConnessione, (void*)NULL) != 0, "creazione gestoreConnessione nella pool");
        stampaDebug("> Thread worker generato.");
    }

    pthread_t sigID;
    // creo il thread che gestisce i segnali
	checkStop(pthread_create(&sigID, NULL, attesaSegnali, (void*)NULL) != 0, "creazione thread per attesa segnali");
	stampaDebug("> Thread gestore segnali generato.");


    /*========= TERMINE THREAD =========*/
    
	// in attesa della terminazione del thread che gestisce i segnali
	checkStop(pthread_join(sigID, NULL) != 0, "join del thread per attesa segnali");
	stampaDebug("> Thread gestore segnali terminato.");
    
    // in attesa della terminazione dei workers nella pool
    for(int i = 0; i < config.max_workers; i++) {
        checkStop(pthread_join(idPool[i], NULL) != 0, "join di un thread nella pool");
        stampaDebug("> Thread worker terminato.");
    }
    free(idPool);

    // elimino il thread
    int returnval = pthread_cancel(idGestore);
    checkStop(returnval != 0 && returnval != 3, "cancellazione thread gestore");

    // in attesa della terminazione del thread gestore
    checkStop(pthread_join(idGestore, NULL) != 0, "join del gestore");
    stampaDebug("> Thread gestore connessioni terminato.");

    checkStop(unlink(config.socket_file_name) == -1, "eliminazione file socket");
    deleteQueue(coda_client);


	printStats("stats.txt");

    free(mutexHT);
    ht_destroy(ht);

    ppf(CLR_INFO); printf("---- FINE SERVER ----\n"); ppff();

    return 0;
}
