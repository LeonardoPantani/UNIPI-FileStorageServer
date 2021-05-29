/**
 * @file utils.h
 * @brief Contiene funzioni di utilità, comprese stampe colorate, debug eccetera.
**/
#ifndef UTILS_H_
#define UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define CLR_IMPORTANT   95
#define CLR_HIGHLIGHT   94
#define CLR_WARNING     93
#define CLR_SUCCESS     92
#define CLR_ERROR       91
#define CLR_INFO        90
#define CLR_DEFAULT     0

#define MAX_LINE_SIZE 1000

/**
 * @brief Dà un valore in base a se il carattere specificato è nella stringa o no.
 * 
 * @param ago       simbolo da cercare
 * @param pagliaio  stringa in cui cercare
 * 
 * @return 1 se il simbolo è trovato, 0 altrimenti
**/
int containsCharacter(char ago, char* pagliaio) {
    for(unsigned int i = 0; i < strlen(pagliaio); i++) {
        if(pagliaio[i] == ago)
            return 1;
    }
    return 0;
}


/**
 * @brief Stampa l'inizio del colore.
 * 
 * @param num codice colore
**/
void ppf(int num) { // stampa inizio colore
    printf("\033[%dm", num);
    fflush(stdout);
}


/**
 * @brief Stampa fine del colore.
**/
void ppff(void) { // stampa fine colore
    printf("\033[0m");
    fflush(stdout);
}


/**
 * @brief Stampa un errore.
 * 
 * @param string    stringa da stampare
**/
void pe(char* string) {
    printf("\033[91m%s: %s (codice %d)\033[0m\n", string, strerror(errno), errno);
    fflush(stdout);
}


/**
 * @brief Controlla se la differenza di 2 timespec è maggiore o uguale a quella di una terza timespec.
 * 
 * @param tempoInizio   primo timespec da controllare
 * @param tempoFine     secondo timespec da controllare
 * @param differenza    terzo timespec che deve essere minore della differenza tra i due
 * 
 * @return 1 se la differenza è maggiore di differenza, 0 altrimenti
**/
int timespecDiff(struct timespec tempoInizio, struct timespec tempoFine, struct timespec differenza) {
    int diffSec = tempoFine.tv_sec - tempoInizio.tv_sec;
    long diffNSec = tempoFine.tv_nsec - tempoInizio.tv_nsec;

    if(diffSec > differenza.tv_sec) {
        return 1;
    } else if(diffSec == differenza.tv_sec) {
        if(diffNSec >= differenza.tv_nsec) {
            return 1;
        } else {
            return 0;
        }
    } else {
        return 0;
    }
}

#endif /* UTILS_H */
