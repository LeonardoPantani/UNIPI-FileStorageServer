/**
 * @file    statistics.h
 * @brief   Contiene l'implementazione della struct statistics per le statistiche del server.
 * @author  Leonardo Pantani
**/

#ifndef STATISTICS_H_
#define STATISTICS_H_

typedef struct {
    int n_read;
    int n_write;
    int n_lock;
    int n_openlock;
    int n_unlock;
    int n_delete;
    int n_close;

    int max_size_reached;
    int max_file_number_reached;
    int n_replace_applied;
    
    int max_concurrent_connections;

    int current_connections;
    int current_bytes_used;
    int current_files_saved;

    int bytes_read;
    int bytes_written;

    int* workerRequests;
} Statistics;

#endif /* STATISTICS_H_ */