#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define CLR_HIGHLIGHT   94
#define CLR_WARNING     93
#define CLR_SUCCESS     92
#define CLR_ERROR       91
#define CLR_INFO        90
#define CLR_DEFAULT     0

#define MAX_LINE_SIZE 1000

enum ErroriConfig {
    ERR_FILEOPENING = -1,
    ERR_INVALIDKEY  = -2,
    ERR_NEGVALUE    = -3,
    ERR_EMPTYVALUE  = -4,
    ERR_UNSETVALUES = -5,
    ERR_ILLEGAL     = -6,
};

/**
 * @brief Dà un valore in base a se il carattere specificato è nella stringa o no.
 * 
 * @param ago       simbolo da cercare
 * @param pagliaio  stringa in cui cercare
 * 
 * @return 1 se il simbolo è trovato, 0 altrimenti
**/
int containsCharacter(char ago, char* pagliaio) {
    for(int i = 0; i < strlen(pagliaio); i++) {
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
}


/**
 * @brief Stampa fine del colore.
**/
void ppff() { // stampa fine colore
    printf("\033[0m");
}


/**
 * @brief Stampa un messaggio colorato.
 * 
 * @param string    stringa da stampare
 * @param num       codice colore
**/
void pp(char* string, int num) {
    printf("\033[%dm%s\033[0m\n", num, string);
}


/**
 * @brief Stampa un errore.
 * 
 * @param string    stringa da stampare
**/
void pe(char* string) {
    printf("\033[91m%s: %s (codice %d)\033[0m\n", string, strerror(errno), errno);
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