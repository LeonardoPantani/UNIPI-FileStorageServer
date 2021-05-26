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
int lockHT = -1;
pthread_mutex_t mutexHT = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t mutexChiusura = PTHREAD_MUTEX_INITIALIZER;

ClientQueue *coda_client = NULL;
pthread_mutex_t mutexCodaClient = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t clientInAttesa = PTHREAD_COND_INITIALIZER;

int chiusuraForte  = FALSE;
int chiusuraDebole = FALSE;

static int checkLimits(Message* msg) {
    if(stats.current_bytes_used + msg->data_length > config.max_memory_size) { // l'inserimento del file causerebbe sforamento limiti memoria
        return -1;
    } else if(stats.current_files_saved + 1 > config.max_files) { // l'inserimento del file causerebbe sforamento limiti quantità file
        return -2;
    } else { // l'inserimento del file non causerebbe problemi
        return 0;
    }
}

static Message* elaboraAzione(int socketConnection, Message* msg) {
    // creo variabile per contenere richieste
    Message* risposta = malloc(sizeof(Message));
    checkStop(risposta == NULL, "malloc risposta");
    // fine variabile per contenere richieste

    setMessage(risposta, AC_UNKNOWN, 0, NULL, NULL, 0);

    switch(msg->action) {
        case AC_OPEN: { // O_CREATE = 1 | O_LOCK = 2 | O_CREATE && O_LOCK = 3
            char* buffer = malloc(msg->data_length);
            buffer = msg->data;

            /* =========== CONTROLLI VARI ================== */
            int controllo = checkLimits(msg), pulizia = FALSE;
            
            if(controllo != 0) {
                ppf(CLR_HIGHLIGHT); printf("GC|EA> Controllo limitazioni: "); if(controllo == -1) { printf("sforamento memoria.\n"); } else if(controllo == -2) {  printf("sforamento limite files.\n"); } fflush(stdout);
                setMessage(risposta, AC_FLUSH_START, 0, NULL, NULL, 0);
                checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio inizio flush");
            }

            while(controllo != 0 && ht->e_num != 0) { // devo vedere se il numero di file non è 0 altrimenti non riesco a trovare un valore vecchio sotto ed esplodo
                pulizia = TRUE;
                hash_elem_t* e = ht_find_old_entry(ht);

                locka(mutexStatistiche);
                if(controllo == -1) {
                    ppf(CLR_ERROR); printf("GC|EA> File '%s' (%ld bytes) eliminato per mettere '%s' nella HT: avrei sforato (%d bytes) il limite di %d bytes\n", e->path, e->data_length, msg->path, (msg->data_length+stats.current_bytes_used), config.max_memory_size); fflush(stdout); ppff();
                    stats.current_bytes_used = stats.current_bytes_used - e->data_length;
                } else if(controllo == -2) {
                    ppf(CLR_ERROR); printf("GC|EA> File '%s' (%ld bytes) eliminato per mettere '%s' nella HT: avrei raggiunto il limite di %d file\n", e->path, e->data_length, msg->path, config.max_files); fflush(stdout); ppff();
                    stats.current_files_saved--;
                }
                unlocka(mutexStatistiche);

                setMessage(risposta, AC_FLUSHEDFILE, 0, e->path, e->data, e->data_length);
                checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio con file flushato");

                ht_remove(ht, e->path);
                free(e);

                // rieseguo controllo
                controllo = checkLimits(msg);
            }

            if(pulizia) {
                setMessage(risposta, AC_FLUSH_END, 0, NULL, NULL, 0);
                checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio fine flush");
            }
            /* =========== FINE CONTROLLI VARI ================== */


            /* =========== CONTROLLO CHE LE FLAG SIANO UTILIZZATE CORRETTAMENTE ================== */
            if(ht_get(ht, msg->path) != NULL) { // viene fatta una create con un path che esiste già
                setMessage(risposta, AC_FILEEXISTS, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printf("GC|EA> Impossibile aggiungere file '%s' nella hash table: esiste già\n", msg->path); ppff();
            } else if (msg->flags == 2 && ht_get(ht, msg->path) == NULL) { // viene fatta la lock su un file che non esiste ancora
                setMessage(risposta, AC_FILENOTEXISTS, 0, NULL, NULL, 0);
                ppf(CLR_ERROR); printf("GC|EA> Impossibile aggiungere file '%s' nella hash table: lock su file che non esiste\n", msg->path); ppff();
            } else {
                /* =========== AGGIUNGO UN FILE (CON LOCK O SENZA LOCK) ================== */
                if(msg->flags == 2 || msg->flags == 3) { // file creato in stato di lock
                    checkStop(ht_put(ht, msg->path, buffer, msg->data_length, socketConnection, (unsigned long)time(NULL), socketConnection) == HT_ERROR, "impossibile allocare memoria al nuovo file (con lock)");
                    ppf(CLR_SUCCESS); printf("GC|EA> Inserito e bloccato file '%s' nella hash table.\n", msg->path); ppff();
                } else { // file creato senza lock
                    checkStop(ht_put(ht, msg->path, buffer, msg->data_length, -1, (unsigned long)time(NULL), socketConnection) == HT_ERROR, "impossibile allocare memoria al nuovo file (no lock)");
                    ppf(CLR_SUCCESS); printf("GC|EA> Inserito file '%s' nella hash table.\n", msg->path); ppff();
                }
                setMessage(risposta, AC_FILERCVD, 0, NULL, NULL, 0);

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
            ppf(CLR_INFO); printf("GC|EA> Ricerca del file '%s' in corso...\n", msg->path); ppff();
            void* dati = ht_get(ht, msg->path);
            int lockato = ht_getLock(ht, msg->path);
            int apertoDa = ht_getOpenBy(ht, msg->path);
            size_t lung = ht_getDataLength(ht, msg->path);
            if(dati != NULL) { // il file esiste
                if(lockato == -1 || lockato == socketConnection) {
                    ppf(CLR_INFO); printf("GC|EA> File '%s' trovato, invio...\n", msg->path); ppff();
                    setMessage(risposta, AC_FILESVD, 0, msg->path, dati, lung);
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

        case AC_READ_MULTIPLE: {
            int i = 0;

            if(ht->e_num == 0) {
                setMessage(risposta, AC_FILENOTEXISTS, 0, NULL, NULL, 0);
            } else {
                setMessage(risposta, AC_START_SEND, 0, NULL, NULL, 0);
                checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio di inizio invio file multipli");

                hash_elem_it it = HT_ITERATOR(ht);
                hash_elem_t* e = ht_iterate(&it);
                // mando un insieme di file, solo quelli per i quali il client ha i permessi di accesso
                while(e != NULL && (i < *msg->data || *msg->data == -1) && (e->lock == -1 || e->lock == socketConnection)) {
                    setMessage(risposta, AC_FILE_SENT, 0, e->path, e->data, e->data_length);
                    checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio con file inviato (multiplo)");

                    e = ht_iterate(&it);
                    i++;
                }

                setMessage(risposta, AC_FINISH_SEND, 0, NULL, NULL, 0);
            }

            break;
        }

        case AC_CLOSE: {
            hash_elem_t* valore = ht_getElement(ht, msg->path);

            if(valore->lock == -1 || valore->lock == socketConnection) { // temp
                checkStop(ht_put(ht, msg->path, valore->data, valore->data_length, -1, valore->updatedDate, -1) == HT_ERROR, "impossibile aggiornare file dalla hash table (close)");
                setMessage(risposta, AC_CLOSED, 0, NULL, NULL, 0);
            } else {
                setMessage(risposta, AC_NOTFORU, 0, NULL, NULL, 0);
            }

            break;
        }

        case AC_UNKNOWN: {
            char* testo = "Hai inviato una richiesta non valida.";
            setMessage(risposta, AC_CANTDO, 0, NULL, NULL, 0);
            break;
        }

        default: {
            char* testo = "Mi dispiace, ma non so gestire questo tipo di richieste ora.";
            setMessage(risposta, AC_CANTDO, 0, NULL, testo, strlen(testo));
        }
    }

    /*
    printf("---- LISTA ELEMENTI ----\n");
    fflush(stdout);
	hash_elem_it it2 = HT_ITERATOR(ht);
	char* k = ht_iterate_keys(&it2);
	while(k != NULL)
	{
		printf("CHIAVE %s | PATH: %s\n", k, ht_getPath(ht, k));
        fflush(stdout);
		k = ht_iterate_keys(&it2);
	}
    */

    return risposta;
}

void* gestoreConnessione(void* unused) {
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
        Message* msg = malloc(sizeof(Message));
        checkStop(msg == NULL, "malloc risposta");
        // fine variabile per contenere richieste
        setMessage(msg, AC_UNKNOWN, 0, NULL, NULL, 0);

        // invio messaggio di benvenuto al client
        Message* benvenuto = malloc(sizeof(Message));
        checkStop(benvenuto == NULL, "malloc benvenuto");

        benvenuto->data_length = 0;
        benvenuto->data = NULL;

        char* testo_msg = "Benvenuto, sono in attesa di tue richieste";

        setMessage(benvenuto, AC_WELCOME, 0, NULL, testo_msg, strlen(testo_msg));

        checkStop(sendMessage(socketConnection, benvenuto) != 0, "errore messaggio iniziale fallito");
        pp("GC> WELCOME inviato al client, attendo una risposta HELLO...", CLR_INFO);

        
        // attendo la msg del client al mio benvenuto
        int esito = readMessage(socketConnection, msg);
        checkStop(esito != 0 || msg->action != AC_HELLO, "risposta WELCOME iniziale")
        ppf(CLR_SUCCESS); fprintf(stdout, "GC> Il client ha risposto al WELCOME: %s. Inizio comunicazione.\n", msg->data); ppff();

        
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

                return (void*)NULL;
            }
            unlocka(mutexChiusura);

            msg = elaboraAzione(socketConnection, msg);

            checkStop(sendMessage(socketConnection, msg) == -1, "errore invio messaggio");

            // richiesta dopo
            esito = readMessage(socketConnection, msg);
        }

        locka(mutexStatistiche);
        stats.current_connections--;
        unlocka(mutexStatistiche);
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

        stampaDebug("GP> In attesa di una connessione...");
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
            free(msg);
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
    checkStop(sigaddset(&insieme, SIGTERM) == -1, "settaggio a 1 della flag per SIGTERM");
    checkStop(sigaddset(&insieme, SIGQUIT) == -1, "settaggio a 1 della flag per SIGQUIT");

    while(TRUE) {
        stampaDebug("GS> In attesa di un segnale...");
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
    checkStop(sigaddset(&insieme, SIGHUP) == -1, "settaggio a 1 della flag per SIGTERM");
    checkStop(sigaddset(&insieme, SIGQUIT) == -1, "settaggio a 1 della flag per SIGQUIT");
    checkStop(pthread_sigmask(SIG_SETMASK, &insieme, NULL) != 0, "rimozione mascheramento iniziale dei segnali ignorati o gestiti in modo custom");


    pp("---- INIZIO SERVER ----", CLR_INFO);
    configLoader(&config);

    // crea coda connessione
    checkStop((coda_client = createQueue(config.max_connections)) == NULL, "creazione coda connessioni");

    // creo tabella hash (salva i file)
    ht = ht_create(config.max_files+10);
    checkStop(ht == NULL, "creazione tabella hash");
    
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

    pp("Connessione pronta!", CLR_SUCCESS);

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


    pp("---- FINE SERVER ----", CLR_INFO);

    return 0;
}