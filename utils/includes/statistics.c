/**
 * @file    statistics.c
 * @brief   Contiene l'implementazione della funzione print per le statistiche del server.
 * @author  Leonardo Pantani
**/

#include <stdio.h>

#include "macro.h"
#include "utils.h"
#include "statistics.h"

static int printStats(const char* pathname) {
    extern Statistics stats;
    extern pthread_mutex_t mutexStatistiche;
    FILE* filePointer;

    filePointer = fopen(pathname, "w");

    checkM1(filePointer == NULL, "impossibile creare file stats");

    char *out = malloc(sizeof(char)*1024);

    time_t tempo;
    char buffer[80];
    time(&tempo);
    strftime(buffer, 80, "%c", localtime(&tempo));

    sprintf(out, "---- STATISTICHE SERVER [Calcolate in data: %s] ----\nLetture totali: %d\nScritture totali: %d\nLock totali: %d\nAperture&Lock totali: %d\nUnlock totali: %d\nCancellazioni totali: %d\nChiusure totali: %d\n\nDimensione max file totale raggiunta: %d\nNumero massimo di file raggiunto: %d\nNumero di applicazioni dell'algoritmo di rimpiazzamento: %d\nNumero di connessioni concorrenti totali: %d\n", buffer, stats.n_read, stats.n_write, stats.n_lock, stats.n_openlock, stats.n_unlock, stats.n_delete, stats.n_close, stats.max_size_reached, stats.max_file_number_reached, stats.n_replace_applied, stats.max_concurrent_connections);

    locka(mutexStatistiche);
    fputs(out, filePointer);
    unlocka(mutexStatistiche);

    fclose(filePointer);
    free(out);

    return 0;
}
