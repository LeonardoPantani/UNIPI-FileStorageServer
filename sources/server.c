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

Config config;

Statistics stats = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
pthread_mutex_t mutexStatistiche = PTHREAD_MUTEX_INITIALIZER;

hashtable_t *ht = NULL;
pthread_mutex_t* mutexHT = NULL;
int nmutex = 0;

pthread_mutex_t mutexChiusura = PTHREAD_MUTEX_INITIALIZER;

List *coda_client = NULL;
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


static int unlockElement(hash_elem_t* el) {
    checkM1(el == NULL, "elemento nullo unlockElement");
    checkM1(el->lock == -1, "elemento non lockato");
    if(el->codaRichiedentiLock == NULL) { // non c'è nessuno che aspetta la lock
        el->lock = -1;
        ppf(CLR_INFO); printSave("UE   > Lock rilasciata perché non c'era nessun altro in coda a cui darla."); ppff();
    } else { // c'è qualcuno in attesa, gli assegno la lock e lo sveglio
        el->lock = removeFromList(el->codaRichiedentiLock);
        checkM1(el->lock == 0, "rimozione dalla coda richiedenti lock fallita");
        printSave("UE   > Lock rilasciata e assegnata al primo della coda.");
        if(el->codaRichiedentiLock->numClients == 0) { // era l'ultimo della coda
            deleteList(el->codaRichiedentiLock);
            el->codaRichiedentiLock = NULL;
            printSave("UE   > Ho prelevato l'ultimo client in attesa, coda eliminata.");
        }
        checkM1(el->rilascioLock == NULL, "variabile condizione nulla unlockElement");
        checkM1(pthread_cond_broadcast(el->rilascioLock) != 0, "risveglio client in attesa");
    }

    return 0;
}


static Message* elaboraAzione(int socketConnection, Message* msg, int numero) {
    // creo variabile per contenere richieste
    Message* risposta = calloc(4, sizeof(Message));
    checkStop(risposta == NULL, "malloc risposta");
    // fine variabile per contenere richieste

    setMessage(risposta, ANS_UNKNOWN, 0, NULL, NULL, 0);

    hash_elem_t* el = ht_getElement(ht, msg->path);

    switch(msg->action) {
        case REQ_OPEN: { // O_CREATE = 1 | O_LOCK = 2 | O_CREATE && O_LOCK = 3
            if(msg->path == NULL) { // controllo che il path non sia vuoto
                setMessage(risposta, ANS_BAD_RQST, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printSave("GC %d > Il client %d ha mandato una OPEN con path nullo.\n", numero, socketConnection); ppff();
                break;
            }

            /* =========== CONTROLLO CHE LE FLAG SIANO UTILIZZATE CORRETTAMENTE ================== */
            if(el != NULL && (msg->flags == 1 || msg->flags == 3)) { // viene fatta una create con un path che esiste già
                setMessage(risposta, ANS_FILE_EXISTS, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printSave("GC %d > Impossibile aggiungere file '%s' nella hash table: esiste già", numero, msg->path); ppff();
            } else if (msg->flags == 2 && el == NULL) { // viene fatta la lock su un file che non esiste ancora
                setMessage(risposta, ANS_FILE_NOT_EXISTS, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printSave("GC %d > Impossibile aggiungere file '%s' nella hash table: lock su file che non esiste", numero, msg->path); ppff();
            } else {
                /* =========== CONTROLLO NUMERO FILE ================== */
                if(msg->flags != 2) { // se richiesta è solo una open con lock allora non c'è bisogno di controllare se sforo il limite di file inseriti
                    int controllo = checkLimits(msg, 2), pulizia = FALSE;
                    
                    if(controllo != 0) {
                        ppf(CLR_HIGHLIGHT); printSave("GC %d > Controllo limitazioni: NUMERO FILE", numero); ppff();
                        setMessage(risposta, ANS_STREAM_START, 0, NULL, NULL, 0);
                        checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio inizio flush");
                    }

                    while(controllo != 0 && ht->e_num != 0) { // devo vedere se il numero di file non è 0 altrimenti non riesco a trovare un valore vecchio sotto ed esplodo
                        pulizia = TRUE;
                        hash_elem_t* e = ht_find_old_entry(ht);
                        
                        // --- Aggiornamento statistiche (file_salvati-- | dati_usati-- | replace_applicati++)
                        locka(mutexStatistiche);
                        stats.current_files_saved--;
                        stats.current_bytes_used = stats.current_bytes_used - e->data_length;
                        stats.n_replace_applied++;
                        unlocka(mutexStatistiche);
                        ppf(CLR_ERROR); printSave("GC %d > File '%s' (%zu bytes) eliminato per inserire '%s' nella HT: avrei raggiunto il limite di %d file", numero, e->path, e->data_length, msg->path, config.max_files); fflush(stdout); ppff();

                        setMessage(risposta, ANS_STREAM_FILE, 0, e->path, e->data, e->data_length);
                        checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio con file flushato");

                        free(ht_remove(ht, e->path));
                        free(e);

                        // rieseguo controllo
                        controllo = checkLimits(msg, 2);
                    }

                    if(pulizia) {
                        setMessage(risposta, ANS_STREAM_END, 0, NULL, NULL, 0);
                        checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio fine flush");
                    }
                }
                /* =========== FINE CONTROLLO NUMERO FILE ================== */

                /* =========== CREO O APRO FILE (CON LOCK O SENZA LOCK) ================== */
                el = ht_getElement(ht, msg->path);
                
                switch(msg->flags) {
                    case 1: { // file creato e aperto (no lock)
                        checkStop(ht_put(ht, msg->path, NULL, 0, -1, (unsigned long)time(NULL)) == HT_ERROR, "creazione e apertura file senza lock");
                        ppf(CLR_SUCCESS); printSave("GC %d > File '%s' inserito nella hash table.", numero, msg->path); ppff();
                        break;
                    }

                    case 2: { // file aperto (lock)
                        if(el != NULL) {
                            el->lock = socketConnection;
                        } else {
                            ppf(CLR_WARNING); printSave("GC %d > Il client ha fatto una lock su un file che è stato cancellato. Lo creo e apro in lock.", numero); ppff();
                            checkStop(ht_put(ht, msg->path, NULL, 0, socketConnection, (unsigned long)time(NULL)) == HT_ERROR, "creazione e apertura file senza lock");
                        }
                        
                        ppf(CLR_SUCCESS); printSave("GC %d > File '%s' aperto e bloccato nella hash table.", numero, msg->path); ppff();
                        break;
                    }

                    case 3: { // file creato e aperto (lock)
                        checkStop(ht_put(ht, msg->path, NULL, 0, socketConnection, (unsigned long)time(NULL)) == HT_ERROR, "creazione e apertura file con lock");
                        ppf(CLR_SUCCESS); printSave("GC %d > File '%s' creato e aperto nella hash table.", numero, msg->path); ppff();
                        locka(mutexStatistiche);
                        stats.n_openlock++;
                        unlocka(mutexStatistiche);
                        break;
                    }

                    default: {
                        setMessage(risposta, ANS_UNKNOWN, 0, NULL, NULL, 0);
                        ppf(CLR_ERROR); printSave("GC %d > Flag non valida!", numero); ppff();
                    }
                }
                setMessage(risposta, ANS_OK, 0, NULL, NULL, 0);


                // aggiorno le statistiche all'immissione di un file
                locka(mutexStatistiche);
                stats.current_files_saved++;
                if(stats.current_files_saved > stats.max_file_number_reached) stats.max_file_number_reached = stats.current_files_saved;
                unlocka(mutexStatistiche);
            }
            break;
        }

        case REQ_READ: {
            if(el != NULL) { // il file esiste
                if(el->lock == -1 || el->lock == socketConnection) {
                    ppf(CLR_SUCCESS); printSave("GC %d > File '%s' trovato!", numero, msg->path); ppff();
                    setMessage(risposta, ANS_OK, 0, msg->path, el->data, el->data_length);
                } else {
                    ppf(CLR_WARNING); printSave("GC %d > File '%s' trovato, ma il client %d non ne possiede i diritti di lettura/scrittura.", numero, msg->path, socketConnection); ppff();
                    setMessage(risposta, ANS_NO_PERMISSION, 0, NULL, NULL, 0);
                }
            } else {
                ppf(CLR_ERROR); printSave("GC %d > File '%s' non trovato, invio messaggio di errore.", numero, msg->path); ppff();
                setMessage(risposta, ANS_FILE_NOT_EXISTS, 0, NULL, NULL, 0);
            }

            locka(mutexStatistiche);
            stats.n_read++;
            stats.bytes_read += el->data_length;
            unlocka(mutexStatistiche);
            break;
        }

        case REQ_WRITE: {
            if(el != NULL && el->lock == socketConnection && (el->data_length == 0 && el->data == NULL)) { // se il file è stato appena creato ed il client corrente ne è l'owner
                /* =========== CONTROLLO LIMITE MEMORIA ================== */
                int controllo = checkLimits(msg, 1), pulizia = FALSE;
                
                if(controllo != 0) {
                    ppf(CLR_HIGHLIGHT); printSave("GC %d > Controllo limitazioni: SFORAMENTO LIMITE FILE", numero); ppff();
                    setMessage(risposta, ANS_STREAM_START, 0, NULL, NULL, 0);
                    checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio inizio flush");
                }

                while(controllo != 0 && ht->e_num != 0) { // devo vedere se il numero di file non è 0 altrimenti non riesco a trovare un valore vecchio sotto ed esplodo
                    pulizia = TRUE;
                    hash_elem_t* e = ht_find_old_entry(ht);

                    // --- Aggiornamento statistiche (file_salvati-- | dati_usati-- | replace_applicati++)
                    locka(mutexStatistiche);
                    stats.current_files_saved--;
                    stats.current_bytes_used = stats.current_bytes_used - e->data_length;
                    stats.n_replace_applied++;
                    unlocka(mutexStatistiche);
                    ppf(CLR_ERROR); printSave("GC %d > File '%s' (%zu bytes) eliminato per scrivere '%s' nella HT: avrei sforato (%d bytes) il limite di %d bytes", numero, e->path, e->data_length, msg->path, (msg->data_length+stats.current_bytes_used), config.max_memory_size); fflush(stdout); ppff();

                    setMessage(risposta, ANS_STREAM_FILE, 0, e->path, e->data, e->data_length);
                    checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio con file flushato");

                    free(ht_remove(ht, e->path));
                    free(e);

                    // rieseguo controllo
                    controllo = checkLimits(msg, 1);
                }

                if(pulizia) {
                    setMessage(risposta, ANS_STREAM_END, 0, NULL, NULL, 0);
                    checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio fine flush");
                }
                /* =========== FINE CONTROLLO MEMORIA ================== */

                el->data = msg->data;
                el->data_length = msg->data_length;

                // aggiorno le statistiche all'immissione di un file
                locka(mutexStatistiche);
                stats.current_bytes_used += msg->data_length;
                if(stats.current_bytes_used > stats.max_size_reached) stats.max_size_reached = stats.current_bytes_used;
                stats.n_write++;
                stats.bytes_written += msg->data_length;
                unlocka(mutexStatistiche);

                setMessage(risposta, ANS_OK, 0, NULL, NULL, 0);
                ppf(CLR_SUCCESS); printSave("GC %d > File '%s' scritto nella hashtable.", numero, msg->path); ppff();
            } else {
                setMessage(risposta, ANS_BAD_RQST, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printSave("GC %d > File '%s' non appena creato, impossibile scriverci.", numero, msg->path); ppff();
            }

            break;
        }

        case REQ_READ_N: {
            int i = 0, bytes_letti = 0;

            if(ht->e_num == 0) {
                setMessage(risposta, ANS_FILE_NOT_EXISTS, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printSave("GC %d > Non ci sono file da mandare. Invio messaggio di errore.", numero); ppff();
            } else {
                setMessage(risposta, ANS_STREAM_START, 0, NULL, NULL, 0);
                checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio di inizio invio file multipli");

                hash_elem_it it = HT_ITERATOR(ht);
                hash_elem_t* e = ht_iterate(&it);
                // mando un insieme di file, solo quelli per i quali il client ha i permessi di accesso
                while(e != NULL && (i < *msg->data || *msg->data == -1) && (e->lock == -1 || e->lock == socketConnection)) {
                    setMessage(risposta, ANS_STREAM_FILE, 0, e->path, e->data, e->data_length);
                    checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio con file inviato (multiplo)");
                    ppf(CLR_SUCCESS); printSave("GC %d > File '%s' inviato.", numero, e->path); ppff();

                    bytes_letti += e->data_length;
                    i++;
                    e = ht_iterate(&it);
                }
                setMessage(risposta, ANS_STREAM_END, 0, NULL, NULL, 0);
                free(msg->data);
            }

            locka(mutexStatistiche);
            stats.n_read += i;
            stats.bytes_read += bytes_letti;
            unlocka(mutexStatistiche);

            break;
        }

        case REQ_CLOSE: {
            if(el->lock == -1 || el->lock == socketConnection) { // puoi chiudere file solo se non c'ha lock oppure se c'è ed è fatto dallo stesso client
                el->lock = -1;
                setMessage(risposta, ANS_OK, 0, NULL, NULL, 0);
                ppf(CLR_SUCCESS); printSave("GC %d > File '%s' chiuso.", numero, msg->path); ppff();

                locka(mutexStatistiche);
                stats.n_close++;
                unlocka(mutexStatistiche);
            } else {
                setMessage(risposta, ANS_NO_PERMISSION, 0, NULL, NULL, 0);
                ppf(CLR_WARNING); printSave("GC %d > Il file '%s' non appartiene al client che ne ha richiesto la chiusura.", numero, msg->path); ppff();
            }

            break;
        }

        case REQ_DELETE: {
            if(el == NULL) {
                setMessage(risposta, ANS_FILE_NOT_EXISTS, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printSave("GC %d > Il file '%s' non esiste. Impossibile eliminarlo.", numero, msg->path); ppff();
            } else {
                if(el->lock == -1 || el->lock == socketConnection) { // puoi cancellare file solo se non c'ha lock oppure se c'è ed è fatto dallo stesso client
                    if(el->codaRichiedentiLock != NULL) {
                        el->lock = -1;
                        checkStop(pthread_cond_broadcast(el->rilascioLock) != 0, "risveglio client in attesa delete")
                    }
                    // --- Aggiornamento statistiche (file_salvati-- | dati_usati-- | replace_applicati++ | n_delete++)
                    locka(mutexStatistiche);
                    stats.current_files_saved--;
                    stats.current_bytes_used = stats.current_bytes_used - el->data_length;
                    stats.n_replace_applied++;
                    stats.n_delete++;
                    unlocka(mutexStatistiche);
                    
                    free(ht_remove(ht, msg->path));
                    setMessage(risposta, ANS_OK, 0, NULL, NULL, 0);
                    ppf(CLR_SUCCESS); printSave("GC %d > File '%s' eliminato.", numero, msg->path); ppff();
                } else {
                    setMessage(risposta, ANS_NO_PERMISSION, 0, NULL, NULL, 0);
                    ppf(CLR_WARNING); printSave("GC %d > Il file '%s' non appartiene al client che ne ha richiesto l'eliminazione.", numero, msg->path); ppff();
                }
            }
            break;
        }

        case REQ_APPEND: {
            if(el->lock == socketConnection) { // se il file non è lockato sull'owner attuale
                /* =========== CONTROLLO LIMITE MEMORIA ================== */
                int controllo = checkLimits(msg, 1), pulizia = FALSE;
                
                if(controllo != 0) {
                    ppf(CLR_HIGHLIGHT); printSave("GC %d > Controllo limitazioni: SFORAMENTO MEMORIA", numero); fflush(stdout);
                    setMessage(risposta, ANS_STREAM_START, 0, NULL, NULL, 0);
                    checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio inizio flush");
                }

                while(controllo != 0 && ht->e_num != 0) { // devo vedere se il numero di file non è 0 altrimenti non riesco a trovare un valore vecchio sotto ed esplodo
                    pulizia = TRUE;
                    hash_elem_t* e = ht_find_old_entry(ht);

                    // --- Aggiornamento statistiche (file_salvati-- | dati_usati-- | replace_applicati++)
                    locka(mutexStatistiche);
                    stats.current_files_saved--;
                    stats.current_bytes_used = stats.current_bytes_used - e->data_length;
                    stats.n_replace_applied++;
                    unlocka(mutexStatistiche);
                    ppf(CLR_ERROR); printSave("GC %d > File '%s' (%zu bytes) eliminato per fare la append su '%s' nella HT: avrei sforato (%d bytes) il limite di %d bytes", numero, e->path, e->data_length, msg->path, (msg->data_length+stats.current_bytes_used), config.max_memory_size); fflush(stdout); ppff();

                    setMessage(risposta, ANS_STREAM_FILE, 0, e->path, e->data, e->data_length);
                    checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio con file flushato");

                    printf("ciao\n");
                    free(ht_remove(ht, e->path));
                    free(e);
                    printf("come stai\n");

                    // rieseguo controllo
                    controllo = checkLimits(msg, 1);
                }

                if(pulizia) {
                    setMessage(risposta, ANS_STREAM_END, 0, NULL, NULL, 0);
                    checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio fine flush");
                }
                /* =========== FINE CONTROLLO MEMORIA ================== */

                
                void* dato = malloc(el->data_length+msg->data_length);
                memset(dato, 0, el->data_length+msg->data_length);
                memcpy(dato, el->data, el->data_length);
                free(el->data);
                memcpy((char*)dato+(el->data_length), msg->data, msg->data_length);

                el->data = dato;
                el->data_length = el->data_length+msg->data_length;
                // aggiorno le statistiche all'immissione di un file
                locka(mutexStatistiche);
                // aggiorno i byte usati
                stats.current_bytes_used = stats.current_bytes_used + msg->data_length;
                if(stats.current_bytes_used > stats.max_size_reached) stats.max_size_reached = stats.current_bytes_used;
                stats.n_write++;
                stats.bytes_written += msg->data_length;
                unlocka(mutexStatistiche);

                setMessage(risposta, ANS_OK, 0, NULL, NULL, 0);
                ppf(CLR_SUCCESS); printSave("GC %d > File '%s' aggiornato nella hashtable.", numero, msg->path); ppff();
            } else {
                setMessage(risposta, ANS_NO_PERMISSION, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printSave("GC %d > File '%s' non appena creato, impossibile scriverci.", numero, msg->path); ppff();
            }
            break;
        }

        case REQ_LOCK: {
            if(el != NULL) {
                if(el->lock == -1 || el->lock == socketConnection) { // il lock non c'è o è già settato dal client che ha fatto richiesta
                    el->lock = socketConnection; // ottengo il lock
                    setMessage(risposta, ANS_OK, 0, NULL, NULL, 0);
                    ppf(CLR_SUCCESS); printSave("GC %d > File '%s' lockato dal client %d.", numero, msg->path, socketConnection); ppff();

                    locka(mutexStatistiche);
                    stats.n_lock++;
                    unlocka(mutexStatistiche);
                } else { // il lock è di qualcun altro
                    if(el->codaRichiedentiLock == NULL) {
                        el->codaRichiedentiLock = createList(config.max_workers);
                        checkStop(el->codaRichiedentiLock == NULL, "creazione coda richiedenti lock REQ_LOCK");
                    }

                    checkStop(addToList(el->codaRichiedentiLock, socketConnection) == -1, "aggiunta client coda richiedenti lock REQ_LOCK");
                
                    if(el->rilascioLock == NULL) {
                        el->rilascioLock = malloc(sizeof(pthread_cond_t));
                        checkStop(el->rilascioLock == NULL, "creazione cond rilasciolock REQ_LOCK");
                        checkStop(pthread_cond_init(el->rilascioLock, NULL), "inizializzazione cond rilasciolock REQ_LOCK");
                    }

                    ppf(CLR_HIGHLIGHT); printSave("GC %d > In attesa di acquisire la lock dell'oggetto...", numero); ppff();

                    while(TRUE) {
                        checkStop(pthread_cond_wait(el->rilascioLock, el->mutex) != 0, "attesa rilascio lock oggetto");
                        ppf(CLR_HIGHLIGHT); printSave("GC %d > Svegliato dall'attesa per la lock!", numero); ppff();

                        el = ht_getElement(ht, msg->path);

                        if(el == NULL) { // quel file non esiste più
                            setMessage(risposta, ANS_FILE_NOT_EXISTS, 0, NULL, NULL, 0);
                            break;
                        }

                        if(el->lock == socketConnection) { // ho ottenuto il lock
                            setMessage(risposta, ANS_OK, 0, NULL, NULL, 0);
                            break;
                        }
                    }
                    ppf(CLR_HIGHLIGHT); printSave("GC %d > Uscito dal while attesa lock.", numero); ppff();

                    if(risposta->action == ANS_FILE_NOT_EXISTS) {
                        printSave("GC %d > Il file è stato cancellato mentre attendevo la lock!", numero);
                        el->codaRichiedentiLock->numClients--;
                    } else {
                        printSave("GC %d > Sono uscito dal while attesa lock e ho ottenuto il lock.", numero);
                        if(el->codaRichiedentiLock == NULL) {
                            free(el->rilascioLock);
                            el->rilascioLock = NULL;
                        }
                    }
                }
            } else {
                setMessage(risposta, ANS_FILE_NOT_EXISTS, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printSave("GC %d > Il file '%s' richiesto dal client non esiste. Impossibile lockarlo.", numero, msg->path); ppff();
            }

            break;
        }

        case REQ_UNLOCK: {
            if(el != NULL) {
                if(el->lock == socketConnection) {
                    if(unlockElement(el) == 0) {
                        setMessage(risposta, ANS_OK, 0, NULL, NULL, 0);
                    } else {
                        setMessage(risposta, ANS_ERROR, 0, NULL, NULL, 0);
                    }
                } else {
                    setMessage(risposta, ANS_NO_PERMISSION, 0, NULL, NULL, 0);
                    ppf(CLR_ERROR); printSave("GC %d > Il file '%s' non può essere unlockato dal client %d perché il lock non è assegnato a lui.", numero, msg->path, socketConnection); ppff();
                }
            } else {
                setMessage(risposta, ANS_FILE_NOT_EXISTS, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printSave("GC %d > Il file '%s' richiesto dal client non esiste. Impossibile unlockarlo.", numero, msg->path); ppff();
            }

            break;
        }

        default: {
            char* testo = "Mi dispiace, non so gestire questa richiesta.";
            setMessage(risposta, ANS_BAD_RQST, 0, NULL, testo, strlen(testo));
        }
    }

    #ifdef DEBUG_VERBOSE
    ppf(CLR_WARNING); printSave("------- LISTA ELEMENTI -------"); ppff();
	hash_elem_it it2 = HT_ITERATOR(ht);
	char* k = ht_iterate_keys(&it2);
	while(k != NULL) {
        el = ht_getElement(ht, k);
		printSave("CHIAVE %s | PATH: %s | LOCK: %d | DATA AGGIORNAMENTO: %ld", k, el->path, el->lock, el->updatedDate);
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
        while(coda_client->numClients == 0 || chiusuraDebole || chiusuraForte) {
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
        Message* msg = calloc(4, sizeof(Message));
        checkStop(msg == NULL, "malloc risposta");
        // fine variabile per contenere richieste
        setMessage(msg, ANS_UNKNOWN, 0, NULL, NULL, 0);

        // invio messaggio di benvenuto al client
        Message* benvenuto = calloc(4, sizeof(Message));
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
            ppf(CLR_INFO); printSave("GC %d > Arrivata richiesta di tipo: %d!", numero, msg->action); ppff();

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

        // faccio una unlock nel caso il client si fosse scordato di farla
        hash_elem_it it = HT_ITERATOR(ht);
        char* k = ht_iterate_keys(&it);
        while(k != NULL) {
            hash_elem_t* el = ht_getElement(ht, k);
            if(el->lock == socketConnection) {
                checkStop(unlockElement(el), "unlockElement uscita ciclo");
                ppf(CLR_WARNING); printSave("GC %d > Il client n°%d ha lasciato una lock sul file '%s'. Glie l'ho tolto.", numero, socketConnection, el->path); ppff();
                locka(mutexStatistiche);
                stats.n_unlock++;
                unlocka(mutexStatistiche);
            }

            k = ht_iterate_keys(&it);
        }

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
        
            Message* msg = malloc(sizeof(Message));
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
            ppf(CLR_WARNING); printf("------- LISTA ELEMENTI -------\n"); ppff();
            hash_elem_it it2 = HT_ITERATOR(ht);
            char* k = ht_iterate_keys(&it2);
            while(k != NULL) {
                hash_elem_t* el = ht_getElement(ht, k);
                printf("CHIAVE %s | PATH: %s | LOCK: %d | DATA AGGIORNAMENTO: %ld\n", k, el->path, el->lock, el->updatedDate);
                fflush(stdout);
                k = ht_iterate_keys(&it2);
            }
            ppf(CLR_WARNING); printf("------ FINE LISTA ELEMENTI ------\n"); ppff();
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
    mutexHT = malloc(nmutex*sizeof(pthread_mutex_t));
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
    pthread_t* idPool = malloc(config.max_workers * sizeof(pthread_t));
    checkStop(idPool == NULL, "malloc id pool di thread");

    stats.workerRequests = (int*)malloc(config.max_workers*sizeof(int));
    int *copyWR = stats.workerRequests;
    for(i = 0; i < config.max_workers; i++) {
        stats.workerRequests[i] = 0;
    }

    // faccio create per ogni worker specificato nel config
    for(i = 0; i < config.max_workers; i++) {
        int *argomento = malloc(sizeof(*argomento));
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
