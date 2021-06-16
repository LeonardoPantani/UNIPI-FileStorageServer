/**
 * @file    file.h
 * @brief   Contiene la definizione del file salvato nel server.
 * @author  Leonardo Pantani
**/

#include "utils.h"
#include "list.h"

typedef struct file {
    char path[MAX_PATH_LENGTH];

    char*   data;
    size_t  data_length;

    int     author;
    int     lock;
    int     lastAction;
    time_t  updatedDate;

    pthread_mutex_t mutex;
    pthread_cond_t  rilascioLock;
    List            codaRichiedentiLock;
} File;