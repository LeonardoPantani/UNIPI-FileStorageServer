/**
 * @file    config.c
 * @brief   Contiene l'implementazione dei metodi per leggere la configurazione.
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

int loadConfig(Config* conf) {
    int v = 0, temp;

	FILE *fp = fopen(CONFIG_PATH, "r");
    if(fp == NULL) {
        errno = EXIT_FAILURE;
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
                        valore[strlen(valore) - 1] = '\0';
                        #ifdef DEBUG
                            ppf(CLR_INFO); printf(">> %s: %s\n", variabile, valore); ppff();
                        #endif
                        if(strcmp(variabile, "!SERVER_KEY") == 0) {
                            if(strcmp(valore, KEY) != 0) { // if server key is not the same of constant then stop
                                return ERR_INVALIDKEY;
                            }
                            v++;
                        } else if(strcmp(variabile, "MAX_WORKERS") == 0) {
                            temp = atoi(valore);
                            if(temp <= 0) {
                                return ERR_NEGVALUE;
                            }
                            (*conf).max_workers = atoi(valore);
                            v++;
                        } else if(strcmp(variabile, "MAX_CONNECTIONS") == 0) {
                            temp = atoi(valore);
                            if(temp <= 0) {
                                return ERR_NEGVALUE;
                            }
                            (*conf).max_connections = atoi(valore);
                            v++;
                        } else if(strcmp(variabile, "MAX_MEMORY_SIZE") == 0) {
                            temp = atoi(valore);
                            if(temp <= 0) {
                                return ERR_NEGVALUE;
                            }
                            (*conf).max_memory_size = atoi(valore);
                            v++;
                        } else if(strcmp(variabile, "MAX_FILES") == 0) {
                            temp = atoi(valore);
                            if(temp <= 0) {
                                return ERR_NEGVALUE;
                            }
                            (*conf).max_files = atoi(valore);
                            v++;
                        } else if(strcmp(variabile, "SOCKET_FILE_NAME") == 0) {
                            strcpy((*conf).socket_file_name, valore);
                            v++;
                        } else {
                            // TODO
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
	fclose(fp);

    if(v == VARIABILI_PREVISTE) {
        return 0;
    } else {
        return ERR_UNSETVALUES; // if the number of variables is less than expected then return error
    }
}

void configLoader(Config* conf) {
    pp("> Caricamento file di config", CLR_INFO);
    switch(loadConfig(conf)) {
        case ERR_FILEOPENING:
            pp("Errore durante l'apertura del file di config.", CLR_ERROR);
            exit(EXIT_FAILURE);
        break;

        case ERR_INVALIDKEY:
            pp("Chiave del server non valida.", CLR_ERROR);
            exit(EXIT_FAILURE);
        break;

        case ERR_NEGVALUE:
            pp("Una o più variabili hanno un valore negativo o pari a 0.", CLR_ERROR);
            exit(EXIT_FAILURE);
        break;

        case ERR_EMPTYVALUE:
            pp("Una o più variabili hanno un valore vuoto.", CLR_ERROR);
            exit(EXIT_FAILURE);
        break;

        case ERR_UNSETVALUES:
            pp("Una o più variabili non sono presenti nel file di config.", CLR_ERROR);
            exit(EXIT_FAILURE);
        break;

        case ERR_ILLEGAL:
            pp("Testo illegale nel file di config.", CLR_ERROR);
            exit(EXIT_FAILURE);
        break;
    }
    pp("> Caricamento config completato.", CLR_INFO);
}