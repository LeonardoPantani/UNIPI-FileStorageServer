/**
 * @file    statistics.c
 * @brief   Contiene l'implementazione della funzione print per le statistiche del server.
 * @author  Leonardo Pantani
**/

#include <stdio.h>

#include "macro.h"
#include "utils.h"
#include "statistics.h"

static int printStats(const char* pathname, int workers) {
    extern Statistics stats;

    FILE* filePointer;

    filePointer = fopen(pathname, "w");

    checkM1(filePointer == NULL, "impossibile creare file stats");

    char *out = malloc(sizeof(char)*1024);

    time_t tempo;
    char buffer[80];
    time(&tempo);
    strftime(buffer, 80, "%c", localtime(&tempo));

    float avg_bytes_read = 0, avg_bytes_written = 0;
    if(stats.n_read != 0) avg_bytes_read = (float)stats.bytes_read/stats.n_read;
    if(stats.n_write != 0) avg_bytes_written = (float)stats.bytes_written/stats.n_write;

    ppf(CLR_HIGHLIGHT);
    printf("----- STATISTICHE SERVER [Calcolate in data: %s] -----\nMedia bytes letti: %g\nMedia bytes scritti: %g\nLetture totali: %d\nScritture totali: %d\nLock totali: %d\nAperture&Lock totali: %d\nUnlock totali: %d\nCancellazioni totali: %d\nChiusure totali: %d\n\nDimensione max file totale raggiunta: %f megabytes\nNumero massimo di file raggiunto: %d\nNumero di applicazioni dell'algoritmo di rimpiazzamento: %d\nNumero di connessioni concorrenti totali: %d\n\nLISTA THREAD:\n", buffer, avg_bytes_read, avg_bytes_written, stats.n_read, stats.n_write, stats.n_lock, stats.n_openlock, stats.n_unlock, stats.n_delete, stats.n_close, (stats.max_size_reached/(double)1000000), stats.max_file_number_reached, stats.n_replace_applied, stats.max_concurrent_connections);
    sprintf(out, "%lu %g %g %d %d %d %d %d %d %d %f %d %d %d\n", (unsigned long)time(NULL), avg_bytes_read, avg_bytes_written, stats.n_read, stats.n_write, stats.n_lock, stats.n_openlock, stats.n_unlock, stats.n_delete, stats.n_close, (stats.max_size_reached/(double)1000000), stats.max_file_number_reached, stats.n_replace_applied, stats.max_concurrent_connections);

    fputs(out, filePointer);
    

    for(int i = 0; i < workers; i++) {
        sprintf(out, "> Richieste servite dal thread %d: %d\n", i, stats.workerRequests[i]);
        printf("%s", out);

        sprintf(out, "%d:%d ", i, stats.workerRequests[i]);
        fputs(out, filePointer);
    }
    printf("----- STATISTICHE SERVER [Calcolate in data: %s] -----\n", buffer);
    ppff();


    fclose(filePointer);
    free(out);

    return 0;
}
