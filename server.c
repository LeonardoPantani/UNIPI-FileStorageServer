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

static Message* elaboraAzione(int socketConnection, MessageHeader* msg_hdr, MessageBody* msg_bdy) {
    Message* risposta = malloc(sizeof(Message));
    checkStop(risposta == NULL, "malloc risposta elaboraAzione");

    setMessageHeader(risposta, AC_UNKNOWN, msg_hdr->abs_path, 0);
    setMessageBody(risposta, 0, NULL);

    switch(msg_hdr->action) {
        case AC_OPEN: { // O_CREATE = 1 | O_LOCK = 2 | O_CREATE && O_LOCK = 3
            char* buffer = malloc(msg_bdy->length);
            buffer = msg_bdy->buffer;

            /* =========== CONTROLLO CHE IL NUMERO DI FILE CON QUELLO NUOVO NON SUPERI IL LIMITE ================== */
            if(stats.current_files_saved + 1 > config.max_files) { // se supererei il numero di file massimi salvati
                hash_elem_t* e = ht_find_old_entry(ht);

                //stampaDebug("flush start");
                setMessageHeader(risposta, AC_FLUSH_START, NULL, 0);
                setMessageBody(risposta, 0, NULL);
                checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio inizio flush");

                ppf(CLR_ERROR); printf("GC|EA> Ho eliminato il file '%s' per inserire il file '%s' nella hash table: avrei raggiunto il limite di %d file in memoria\n", e->path, msg_hdr->abs_path, config.max_files); ppff();

                ht_remove(ht, e->key);
                locka(mutexStatistiche);
                stats.current_files_saved--;
                unlocka(mutexStatistiche);

                //stampaDebug("flush file");
                setMessageHeader(risposta, AC_FLUSHEDFILE, e->path, 0);
                setMessageBody(risposta, strlen(e->data), e->data);
                checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio con file flushato");

                //stampaDebug("flush end");
                setMessageHeader(risposta, AC_FLUSH_END, NULL, 0);
                setMessageBody(risposta, 0, NULL);
                checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio fine flush");
                free(e);
            }

            /* =========== CONTROLLO CHE IL NUMERO DI BYTE UTILIZZATI NON SUPERI IL LIMITE ================== */
            int pulizia = FALSE;
            if(stats.current_bytes_used + msg_bdy->length > config.max_memory_size) {
                pulizia = TRUE;
                setMessageHeader(risposta, AC_FLUSH_START, NULL, 0);
                setMessageBody(risposta, 0, NULL);
                checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio inizio flush");
            }

            while(stats.current_bytes_used + msg_bdy->length > config.max_memory_size) { // se supererei il limite di byte salvati libero spazio
                hash_elem_t* e = ht_find_old_entry(ht);

                ppf(CLR_ERROR); printf("GC|EA> Ho eliminato il file '%s' (%ld bytes) per inserire il file '%s' nella hash table: avrei sforato (%ld bytes) il limite di %d bytes occupati in memoria\n", e->path, strlen(e->data), msg_hdr->abs_path, (strlen(e->data)+stats.current_bytes_used), config.max_memory_size); ppff();

                ht_remove(ht, e->key);
                locka(mutexStatistiche);
                stats.current_bytes_used = stats.current_bytes_used - msg_bdy->length;
                unlocka(mutexStatistiche);

                //stampaDebug("flush file");
                setMessageHeader(risposta, AC_FLUSHEDFILE, e->path, 0);
                setMessageBody(risposta, msg_bdy->length, msg_bdy->buffer);
                checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio con file flushato");
                free(e);
            }

            if(pulizia) {
                //stampaDebug("flush end");
                setMessageHeader(risposta, AC_FLUSH_END, NULL, 0);
                setMessageBody(risposta, 0, NULL);
                checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio fine flush");
            }

            /* =========== CONTROLLO CHE LE FLAG SIANO UTILIZZATE CORRETTAMENTE ================== */
            if(ht_get(ht, msg_hdr->abs_path) != NULL) { // viene fatta una create con un path che esiste già
                setMessageHeader(risposta, AC_FILEEXISTS, msg_hdr->abs_path, 0);
                ppf(CLR_ERROR); printf("GC|EA> Impossibile aggiungere file '%s' nella hash table: esiste già\n", msg_hdr->abs_path); ppff();
            } else if (msg_hdr->flags == 2 && ht_get(ht, msg_hdr->abs_path) == NULL) { // viene fatta la lock su un file che non esiste ancora
                setMessageHeader(risposta, AC_FILENOTEXISTS, msg_hdr->abs_path, 0);
                ppf(CLR_ERROR); printf("GC|EA> Impossibile aggiungere file '%s' nella hash table: lock su file che non esiste\n", msg_hdr->abs_path); ppff();
            } else {
                /* =========== AGGIUNGO UN FILE (CON LOCK O SENZA LOCK) ================== */
                if(msg_hdr->flags == 2 || msg_hdr->flags == 3) { // file creato in stato di lock
                    checkStop(ht_put(ht, msg_hdr->abs_path, buffer, socketConnection, (unsigned long)time(NULL), socketConnection) == HT_ERROR, "impossibile allocare memoria al nuovo file (con lock)");
                    ppf(CLR_SUCCESS); printf("GC|EA> Inserito e bloccato file '%s' nella hash table.\n", msg_hdr->abs_path); ppff();
                } else { // file creato senza lock
                    checkStop(ht_put(ht, msg_hdr->abs_path, buffer, -1, (unsigned long)time(NULL), socketConnection) == HT_ERROR, "impossibile allocare memoria al nuovo file (no lock)");
                    ppf(CLR_SUCCESS); printf("GC|EA> Inserito file '%s' nella hash table.\n", msg_hdr->abs_path); ppff();
                }
                setMessageHeader(risposta, AC_FILERCVD, msg_hdr->abs_path, 0);

                // aggiorno le statistiche all'immissione di un file
                locka(mutexStatistiche);
                // aggiorno i file salvati
                stats.current_files_saved++;
                if(stats.current_files_saved > stats.max_file_number_reached) {
                    stats.max_file_number_reached = stats.current_files_saved;
                }
                // aggiorno i byte usati
                stats.current_bytes_used = stats.current_bytes_used + msg_bdy->length;
                if(stats.current_bytes_used > stats.max_size_reached) {
                    stats.max_size_reached = stats.current_bytes_used;
                }
                unlocka(mutexStatistiche);
            }
            break;
        }

        case AC_READ: {
            printf("Ricerca del file con percorso '%s' in corso...\n", msg_hdr->abs_path);
            void* dati = ht_get(ht, msg_hdr->abs_path);
            if(dati != NULL) { // mando il file
                setMessageHeader(risposta, AC_FILESVD, msg_hdr->abs_path, 0);
                setMessageBody(risposta, strlen(dati), dati);      
            } else {
                setMessageHeader(risposta, AC_FILENOTEXISTS, msg_hdr->abs_path, 0);
                setMessageBody(risposta, 0, NULL);
            }
            break;
        }

        case AC_UNKNOWN: {
            char* testo = "Hai inviato una richiesta non valida.";
            setMessageHeader(risposta, AC_CANTDO, NULL, 0);
            setMessageBody(risposta, strlen(testo), testo);
            break;
        }

        default: {
            char* testo = "Mi dispiace, ma non so gestire questo tipo di richieste ora.";
            setMessageHeader(risposta, AC_CANTDO, NULL, 0);
            setMessageBody(risposta, strlen(testo), testo);
        }
    }

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
        // creo variabili per contenere risposte
        MessageHeader* header = malloc(sizeof(MessageHeader));
        checkStop(header == NULL, "malloc header");

        MessageBody* body = malloc(sizeof(MessageBody));
        checkStop(body == NULL, "malloc body");

        Message* risposta = malloc(sizeof(Message));
        checkStop(risposta == NULL, "malloc risposta");
        setMessageHeader(risposta, AC_UNKNOWN, NULL, 0);
        setMessageBody(risposta, 0, NULL);

        // invio messaggio di benvenuto al client
        Message* benvenuto = malloc(sizeof(Message));
        checkStop(benvenuto == NULL, "malloc benvenuto");

        benvenuto->bdy.length = 0;
        benvenuto->bdy.buffer = NULL;

        char* testo_risposta = "Benvenuto, sono in attesa di tue richieste";

        setMessageHeader(benvenuto, AC_WELCOME, NULL, 0);
        setMessageBody(benvenuto, strlen(testo_risposta), testo_risposta);

        checkStop(sendMessage(socketConnection, benvenuto) != 0, "errore messaggio iniziale fallito");
        pp("GC> Benvenuto inviato al client, attendo una risposta...", CLR_INFO);

        
        // attendo la risposta del client al mio benvenuto
        int esito = readMessageHeader(socketConnection, header);
        if(esito == 0) {
            if(header->action == AC_HELLO) {
                if(readMessageBody(socketConnection, body) != 0) {
                    pp("GC> Il client ha mandato un AC_HELLO senza body non valido!", CLR_ERROR);
                    break;
                }
                ppf(CLR_SUCCESS); fprintf(stdout, "GC> Il client ha risposto al WELCOME: %s. Inizio comunicazione.\n", body->buffer); ppff();
            }
        }

        
        // qui inizia la comunicazione dopo la conferma dei messaggi WELCOME ED HELLO
        esito = readMessageHeader(socketConnection, header);
        while(esito == 0) {
            ppf(CLR_INFO); printf("GC> Arrivata richiesta di tipo: %d!\n", header->action); ppff();
            // controllo se c'è un segnale di terminazione
            locka(mutexChiusura);
            if(chiusuraForte) {
                unlocka(mutexChiusura);
                setMessageHeader(risposta, AC_STOPPING, NULL, 0);
                checkStop(sendMessage(socketConnection, risposta) != 0, "errore messaggio iniziale fallito");
                checkStop(close(socketConnection) == -1, "chiusura connessione dopo segnale");

                free(header);
                free(body);
                return (void*)NULL;
            }
            unlocka(mutexChiusura);

            stampaDebug("priam del body");
            int esito2 = readMessageBody(socketConnection, body);
            stampaDebug("dopo il body");
            risposta = elaboraAzione(socketConnection, header, body);

            checkStop(sendMessage(socketConnection, risposta) == -1, "errore invio messaggio");

            // richiesta dopo
            esito = readMessageHeader(socketConnection, header);
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
        
            Message* risposta = malloc(sizeof(Message));
            checkStop(risposta == NULL, "malloc errore max connessioni");

            setMessageHeader(risposta, AC_MAXCONNECTIONSREACHED, NULL, 0);

            risposta->bdy.length = 0;
            risposta->bdy.buffer = NULL;

            checkStop(sendMessage(socketConnection, risposta) != 0, "messaggio connessioni max raggiunto");
            free(risposta);
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
    ht = ht_create(config.max_files);
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