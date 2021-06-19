/**
 * @file    utils.h
 * @brief   Contiene l'implementazione di alcune funzioni di utilità (stampe colorate, debug, comparazioni di stringhe custom e altro)
 * @author  Leonardo Pantani
**/

#include "utils.h"

void* cmalloc(size_t size) {
    void *ret = malloc(size);

    if(ret == NULL) {
        perror("Errore allocazione memoria");
        exit(EXIT_FAILURE);
    }

    memset(ret, 0, size);
    return ret;
}

int containsCharacter(char ago, char* pagliaio) {
    for(unsigned int i = 0; i < strlen(pagliaio); i++) {
        if(pagliaio[i] == ago)
            return 0;
    }
    return -1;
}


void ppf(int num) { // stampa inizio colore
    printf("\033[%dm", num);
    fflush(stdout);
}


void ppff(void) { // stampa fine colore
    printf("\033[0m");
    fflush(stdout);
}


void pe(char* string) {
    printf("\033[91m%s: %s (codice %d)\033[0m\n", string, strerror(errno), errno);
    fflush(stdout);
}


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


int strcmpnl(const char *s1, const char *s2) {
    char s1c;
    char s2c;
    
    do {
        s1c = *(s1++);
        s2c = *(s2++);
        if (s1c == '\n') s1c = 0;
        if (s2c == '\n') s2c = 0;
        if (s1c != s2c) return -1;
    } while (s1c);

    return 0;
}