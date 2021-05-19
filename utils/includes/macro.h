/**
 * @file    macro.h
 * @brief   Implementa alcune define di utilità, principalmente per la stampa di errori e per velocizzare la scrittura del codice.
**/

#ifndef MACRO_H_
#define MACRO_H_

#include <stdio.h>
#include <string.h>
#include <errno.h>

#define FALSE 0
#define TRUE 1

#define checkStop(condizione, messaggio) if(condizione) { fprintf(stderr, "Errore %s (codice %d) | File %s Riga %d (%s)\n", strerror(errno), errno, __FILE__, __LINE__, messaggio); fflush(stderr); exit(EXIT_FAILURE); }
#define checkNull(condizione, messaggio) if(condizione) { fprintf(stderr, "Errore %s (codice %d) | File %s Riga %d (%s)\n", strerror(errno), errno, __FILE__, __LINE__, messaggio); fflush(stderr); return NULL; }
#define checkM1(condizione, messaggio) if(condizione) { fprintf(stderr, "Errore %s (codice %d) | File %s Riga %d (%s)\n", strerror(errno), errno, __FILE__, __LINE__, messaggio); fflush(stderr); return -1; }

#ifdef DEBUG
    #define stampaDebug(testo) if(DEBUG) { fprintf(stderr, "%s\n", testo); fflush(stderr); } else { }
#else
    #define stampaDebug(testo) {}
#endif

#define locka(mutex) if(pthread_mutex_lock(&mutex) != 0) { fprintf(stderr, "Lock semaforo fallita | File %s Riga %d\n", __FILE__, __LINE__); exit(EXIT_FAILURE); }
#define unlocka(mutex) if(pthread_mutex_unlock(&mutex) != 0) { fprintf(stderr, "Unlock semaforo fallita | File %s Riga %d\n", __FILE__, __LINE__); exit(EXIT_FAILURE); }

#endif /* MACRO_H_ */