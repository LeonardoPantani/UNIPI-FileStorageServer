#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "utils/includes/actions.h"
#include "api.c"

int verbose = FALSE; // stampa per ogni operazione

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

        int socket_fd;
        struct timespec tempoMassimo;
        tempoMassimo.tv_nsec = 0;
        tempoMassimo.tv_sec  = 15;

        if((socket_fd = openConnection(socket_path, 5000, tempoMassimo)) == -1) {
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

        sleep(10);
		
		return 0;
	} else {
		pp("Fornisci almeno un argomento. Usa -h se non sei sicuro.", CLR_ERROR);
		return 0;
	}
    return 0;
}