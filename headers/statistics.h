/**
 * @file    statistics.h
 * @brief   Contiene l'implementazione della struct statistics per le statistiche del server.
 * @author  Leonardo Pantani
**/

#ifndef STATISTICS_H_
#define STATISTICS_H_

#include <stdio.h>

#include "config.h"
#include "utils.h"

/**
 * @brief   Struttura che definisce l'insieme di statistiche salvate dal server.
*/
typedef struct statistics {
    int n_read;
    int n_write;
    int n_lock; // non usata
    int n_open;
    int n_opencreate;
    int n_unlock; // non usata
    int n_delete;
    int n_close;

    int max_size_reached;
    int max_file_number_reached;
    int n_replace_applied;
    
    int max_concurrent_connections;

    int blocked_connections;
    int current_connections;
    int current_bytes_used;
    int current_files_saved;

    int bytes_read;
    int bytes_written;

    int* workerRequests; // contiene l'array contenente le richieste servite da ogni worker
} Statistics;

/**
 * @brief   Stampa le statistiche del server a schermo
 *          e le scrive sul file pathname.
 * 
 * @param   pathname    Il percorso del file su cui scrivere le statistiche
 * @param   workers     Il numero di workers di cui stampare le statistiche
**/
int printStats(const char* pathname, int workers);

#endif /* STATISTICS_H_ */