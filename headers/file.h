/**
 * @file    file.h
 * @brief   Contiene la definizione del file salvato nel server.
 * @author  Leonardo Pantani
**/

#include "utils.h"
#include "list.h"

typedef struct file {
    char path[PATH_MAX];

    char*   data;
    size_t  data_length;

    int     author;
    int     lastAction;
    int     openBy;
    time_t  updatedDate;
} File;