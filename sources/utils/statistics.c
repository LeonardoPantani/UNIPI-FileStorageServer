/**
 * @file    statistics.c
 * @brief   Contiene l'implementazione della funzione print per le statistiche del server.
 * @author  Leonardo Pantani
**/

#include "statistics.h"

#define BUFFER_STATS_SIZE 1024 // dimensione del buffer che contiene le statistiche da mettere nel file

int printStats(const char* pathname, int workers) {
    extern Statistics stats;
    extern Config config;
    extern FILE* fileLog;
    extern pthread_mutex_t mutexFileLog;

    FILE* filePointer;
    filePointer = fopen(pathname, "w");
    checkM1(filePointer == NULL, "impossibile creare file stats");

    // malloc del buffer da usare per scrivere su file
    char *out = cmalloc(sizeof(char)*BUFFER_STATS_SIZE);

    // ottengo la data attuale in formato leggibile
    time_t tempo;
    char buffer[MAX_TIMESTAMP_SIZE];
    time(&tempo);
    strftime(buffer, MAX_TIMESTAMP_SIZE, "%c", localtime(&tempo));

    float avg_bytes_read = 0, avg_bytes_written = 0;
    if(stats.n_read != 0) avg_bytes_read = (float)stats.bytes_read/stats.n_read;
    if(stats.n_write != 0) avg_bytes_written = (float)stats.bytes_written/stats.n_write;

    ppf(CLR_HIGHLIGHT);
    // stampa su schermo
    printSave("----- STATISTICHE SERVER [Calcolate in data: %s] -----\nMedia bytes letti: %g\nMedia bytes scritti: %g\nLetture totali: %d\nScritture totali: %d\nLock totali: %d\nAperture totali: %d\nAperture&Create totali: %d\nUnlock totali: %d\nCancellazioni totali: %d\nChiusure totali: %d\n\nDimensione max file totale raggiunta: %f megabytes/%d megabytes\nNumero massimo di file raggiunto: %d/%d\nNumero di applicazioni dell'algoritmo di rimpiazzamento: %d\nNumero di connessioni concorrenti totali: %d\nNumero di connessioni bloccate: %d\n\nLISTA THREAD:", buffer, avg_bytes_read, avg_bytes_written, stats.n_read, stats.n_write, stats.n_lock, stats.n_open, stats.n_opencreate, stats.n_unlock, stats.n_delete, stats.n_close, (stats.max_size_reached/(double)1000000), config.max_memory_size, stats.max_file_number_reached, config.max_files, stats.n_replace_applied, stats.max_concurrent_connections, stats.blocked_connections);
    // salvataggio nel buffer
    sprintf(out, "%lu %g %g %d %d %d %d %d %d %d %d %d %d %d %d %d\n", (unsigned long)time(NULL), avg_bytes_read, avg_bytes_written, stats.n_read, stats.n_write, stats.n_lock, stats.n_open, stats.n_opencreate, stats.n_unlock, stats.n_delete, stats.n_close, stats.max_size_reached, stats.max_file_number_reached, stats.n_replace_applied, stats.max_concurrent_connections, stats.blocked_connections);

    // scrittura del buffer su file
    fputs(out, filePointer);
    
    // stampa a schermo e scrittura su file delle richieste servite da ogni worker
    for(int i = 0; i < workers; i++) {
        sprintf(out, "> Richieste servite dal thread %d: %d", i, stats.workerRequests[i]);
        printSave("%s", out);

        sprintf(out, "%d:%d ", i, stats.workerRequests[i]);
        fputs(out, filePointer);
    }


    fclose(filePointer);
    free(out);

    return 0;
}
