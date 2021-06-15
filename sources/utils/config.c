/**
 * @file    config.c
 * @brief   Contiene l'implementazione dei metodi per leggere la configurazione.
 * @author  Leonardo Pantani
**/

#include "config.h"

extern FILE* fileLog;
extern pthread_mutex_t mutexFileLog;

int loadConfig(Config* conf, char* posConfig) {
    int v = 0, temp;

	FILE *fp = fopen(posConfig, "r");
    if(fp == NULL) {
        return ERR_FILEOPENING;
    }
	
    char *riga = NULL;
    size_t lun = 0;
    char *variabile, *valore;

    char *saveptr;

    while(getline(&riga, &lun, fp) != -1) { // getting line
        if(riga[0] != '#' && riga[0] != '\n') { // here the row can be a comment or new line
            if(containsCharacter('=', riga) && riga[0] != '=') { // here must be a variabile (or an illegal value)
                variabile = strtok_r(riga, "=", &saveptr);
                if(variabile != NULL) {
                    valore = strtok_r(NULL, "=", &saveptr);
                    if(valore != NULL) {
                        valore[strlen(valore)-1] = '\0';
                        #ifdef DEBUG
                            ppf(CLR_INFO); printSave(">> %s: %s", variabile, valore); ppff();
                        #endif
                        if(strcmpnl(variabile, "!SERVER_KEY") == 0) {
                            if(strcmpnl(valore, KEY) != 0) { // if server key is not the same of constant then stop
                                return ERR_INVALIDKEY;
                            }
                            v++;
                        } else if(strcmpnl(variabile, "MAX_WORKERS") == 0) {
                            temp = atoi(valore);
                            if(temp <= 0) {
                                return ERR_NEGVALUE;
                            }
                            (*conf).max_workers = atoi(valore);
                            v++;
                        } else if(strcmpnl(variabile, "MAX_CONNECTIONS") == 0) {
                            temp = atoi(valore);
                            if(temp <= 0) {
                                return ERR_NEGVALUE;
                            }
                            (*conf).max_connections = atoi(valore);
                            v++;
                        } else if(strcmpnl(variabile, "MAX_MEMORY_SIZE") == 0) {
                            temp = atoi(valore);
                            if(temp <= 0) {
                                return ERR_NEGVALUE;
                            }
                            (*conf).max_memory_size = atoi(valore);
                            v++;
                        } else if(strcmpnl(variabile, "MAX_FILES") == 0) {
                            temp = atoi(valore);
                            if(temp <= 0) {
                                return ERR_NEGVALUE;
                            }
                            (*conf).max_files = atoi(valore);
                            v++;
                        } else if(strcmpnl(variabile, "SOCKET_FILE_NAME") == 0) {
                            strcpy((*conf).socket_file_name, valore);
                            v++;
                        } else if(strcmpnl(variabile, "LOG_FILE_NAME") == 0) {
                            strcpy((*conf).log_file_name, valore);
                            v++;
                        } else if(strcmpnl(variabile, "STATS_FILE_NAME") == 0) {
                            strcpy((*conf).stats_file_name, valore);
                            v++;
                        }
                    } else {
                        return ERR_EMPTYVALUE; // value after = is empty
                    }
                }
            } else {
                return ERR_ILLEGAL; // illegal character or variable (=123 is illegal)
            }
        }
    }
    free(riga);
	fclose(fp);

    if(v == VARIABILI_PREVISTE) {
        return 0;
    } else {
        return ERR_UNSETVALUES; // if the number of variables is less than expected then return error
    }
}

void configLoader(Config* conf, char* posConfig) {
    ppf(CLR_INFO); printf("> Caricamento file di config...\n"); ppff();

    checkStop(posConfig == NULL, "posizione file di config inesistente");

    switch(loadConfig(conf, posConfig)) {
        case ERR_FILEOPENING:
            ppf(CLR_ERROR); pe("> Errore durante l'apertura del file di config"); ppff();
            exit(EXIT_FAILURE);
        break;

        case ERR_INVALIDKEY:
            ppf(CLR_ERROR); printf("> Chiave del server non valida.\n"); ppff();
            exit(EXIT_FAILURE);
        break;

        case ERR_NEGVALUE:
            ppf(CLR_ERROR); printf("> Una o più variabili hanno un valore negativo o pari a 0.\n"); ppff();
            exit(EXIT_FAILURE);
        break;

        case ERR_EMPTYVALUE:
            ppf(CLR_ERROR); printf("> Una o più variabili hanno un valore vuoto.\n"); ppff();
            exit(EXIT_FAILURE);
        break;

        case ERR_UNSETVALUES:
            ppf(CLR_ERROR); printf("> Una o più variabili non sono presenti nel file di config.\n"); ppff();
            exit(EXIT_FAILURE);
        break;

        case ERR_ILLEGAL:
            ppf(CLR_ERROR); printf("> Testo illegale nel file di config.\n"); ppff();
            exit(EXIT_FAILURE);
        break;
    }
    ppf(CLR_INFO); printf("> Caricamento config completato.\n"); ppff();
}