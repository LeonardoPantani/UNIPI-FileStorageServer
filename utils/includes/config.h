/**
 * @file    config.h
 * @brief   Contiene l'header delle funzioni per caricare il config e la struttura che contiene il config.
**/

#ifndef CONFIG_H_
#define CONFIG_H_

#include "utils.h"

#define CONFIG_PATH "./config.txt"
#define KEY "efhiauefrwhwafghaw2131guys"

#define VARIABILI_PREVISTE 6 // va considerata anche la chiave del server, quindi è +1

enum ErroriConfig {
    ERR_FILEOPENING = -1,
    ERR_INVALIDKEY  = -2,
    ERR_NEGVALUE    = -3,
    ERR_EMPTYVALUE  = -4,
    ERR_UNSETVALUES = -5,
    ERR_ILLEGAL     = -6,
};

typedef struct configurazione {
    int max_workers;
    int max_connections;
    int max_memory_size;
    int max_files;
    char socket_file_name[MAX_LINE_SIZE];
} Config;

/**
 * @brief Funzione interna.
 * 
 * @param conf  Puntatore su cui salvare i dati presi dal config.txt
*/
int loadConfig(Config* conf);

/**
 * @brief Carica il file di configurazione.
 * 
 * @param conf  Puntatore su cui salvare i dati presi dal config.txt
**/
void configLoader(Config* conf);

#endif /* CONFIG_H_ */