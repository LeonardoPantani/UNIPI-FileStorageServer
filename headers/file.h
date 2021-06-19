/**
 * @file    file.h
 * @brief   Contiene la definizione del file salvato nel server.
 * @author  Leonardo Pantani
**/

#include "utils.h"
#include "list.h"

/**
 * @brief Struttura che definisce un file salvato nel server.
 * 
 * @struct   path        Percorso del file
 * @struct   data        Dato salvato
 * @struct   data_length Lunghezza del dato
 * @struct   author      Autore del file
 * @struct   lastAction  Codice dell'ultima operazione eseguita su questo file
 * @struct   openBy      Filedescriptor dell'ultimo client che ha aperto questo file
 * @struct   updatedDate Data unix di aggiornamento del file (interazione)
*/
typedef struct file {
    char path[PATH_MAX];

    char*   data;
    size_t  data_length;

    int     author;
    int     lastAction;
    int     openBy;
    time_t  updatedDate;
} File;