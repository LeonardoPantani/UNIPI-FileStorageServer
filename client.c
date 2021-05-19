#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include "api.c"
#include "utils/includes/message.c"
#include "communication.c"

int verbose = FALSE; // stampa per ogni operazione

pthread_mutex_t mutexChiusura = PTHREAD_MUTEX_INITIALIZER;
int chiusuraForte  = FALSE;

typedef struct {
    ActionType  action;
    char*       abs_path;
    int         size;
} Operation;

void help() {
    ppf(CLR_SUCCESS); printf("Utilizzo:\n"); ppff();
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

int main(int argc, char* argv[]) {
    // ---- MASCHERO SEGNALI
    sigset_t insieme;
    checkStop(sigfillset(&insieme) == -1, "inizializzazione set segnali");
    checkStop(pthread_sigmask(SIG_SETMASK, &insieme, NULL) != 0, "mascheramento iniziale segnali");
    // tolgo la maschera inserita all'inizio, la lascio solo per i segnali da gestire con thread
    checkStop(sigemptyset(&insieme) == -1, "settaggio a 0 di tutte le flag di insieme");
    checkStop(sigaddset(&insieme, SIGINT) == -1, "settaggio a 1 della flag per SIGINT");
    checkStop(sigaddset(&insieme, SIGQUIT) == -1, "settaggio a 1 della flag per SIGQUIT");
    // TODO
    checkStop(pthread_sigmask(SIG_SETMASK, &insieme, NULL) != 0, "rimozione mascheramento iniziale dei segnali ignorati o gestiti in modo custom");

	if(argc > 1) {
		int opt;
        char* socket_path = NULL;

		while ((opt = getopt(argc,argv, ":hf:w:W:D:r:R:d:t:l:u:c:p")) != -1) {
			switch(opt) {
				case 'h':
                    help();
                break;
                
                case 'f':
                    socket_path = optarg;
                break;

                case 'p':
                    verbose = TRUE;
                break;

				case ':': // manca un argomento
					ppf(CLR_ERROR); printf("Argomento %c non valido\n", optopt); ppff();
				break;

				case '?': // opzione non valida
					ppf(CLR_ERROR); printf("Argomento %c non riconosciuto\n", optopt); ppff();
				break;

				default:

                break;
			}
		}

        if(socket_path == NULL) {
            return -1;
        }

        int socketConnection;
        struct timespec tempoMassimo;
        tempoMassimo.tv_nsec = 0;
        tempoMassimo.tv_sec  = 10;
        

        if((socketConnection = openConnection(socket_path, 5000, tempoMassimo)) == -1) {
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
        // creo variabili per contenere risposte
        MessageHeader* header = malloc(sizeof(MessageHeader));
        checkStop(header == NULL, "malloc header");

        MessageBody* body = malloc(sizeof(MessageBody));
        checkStop(body == NULL, "malloc body");

        Message* risposta = malloc(sizeof(Message));
        checkStop(risposta == NULL, "malloc risposta");
        setMessageHeader(risposta, AC_UNKNOWN, NULL);
        setMessageBody(risposta, 0, NULL);

        // Attendo il messaggio di benvenuto dal server
        int esito = readMessageHeader(socketConnection, header);

        if(esito == 0) {
            if(header->action == AC_WELCOME) {
                checkStop(readMessageBody(socketConnection, body) != 0, "welcome senza body");

                char* testo_risposta = "Grazie, ci sono";

                setMessageHeader(risposta, AC_HELLO, "*unused*");
                setMessageBody(risposta, strlen(testo_risposta), testo_risposta);

                ppf(CLR_SUCCESS); printf("Client> Ricevuto WELCOME dal server: %s\n", body->buffer); ppff();

                ppf(CLR_INFO); printf("Client> Rispondo al WELCOME con: %s\n", testo_risposta);

                checkStop(sendMessage(socketConnection, risposta) != 0, "risposta hello al server iniziale");

                pp("Client> Inviata risposta al server con successo!", CLR_SUCCESS);
            }
        }

        // comunicazione iniziata!
        stampaDebug("Aspetto prima richiesta");
        esito = readMessageHeader(socketConnection, header);
        while(esito == 0) {
            if(header->action == AC_STOPPING) {
                pp("Client> Il server si sta arrestando, termino la comunicazione.", CLR_HIGHLIGHT);
                break;
            }

            locka(mutexChiusura);
            if(chiusuraForte) {
                unlocka(mutexChiusura);
                checkStop(close(socketConnection) == -1, "chiusura connessione dopo segnale");
                return 0;
            }
            unlocka(mutexChiusura);
            esito = readMessageHeader(socketConnection, header);
        }
        close(socketConnection);
        pp("Client> Connessione terminata.", CLR_INFO);
		
		return 0;
	} else {
		pp("Fornisci almeno un argomento. Usa -h se non sei sicuro.", CLR_ERROR);
		return 0;
	}
    return 0;
}