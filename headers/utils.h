/**
 * @file    utils.h
 * @brief   Contiene funzioni di utilità, comprese stampe colorate, debug, comparazioni di stringhe custom e altro.
 * @author  Leonardo Pantani
**/

#ifndef UTILS_H_
#define UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/limits.h>
#include <pthread.h>

#define FALSE 0
#define TRUE 1

#define CLR_IMPORTANT   95
#define CLR_HIGHLIGHT   94
#define CLR_WARNING     93
#define CLR_SUCCESS     92
#define CLR_ERROR       91
#define CLR_INFO        90
#define CLR_DEFAULT     0

#define MAX_TIMESTAMP_SIZE 30   // dimensione del buffer che contiene la data in formato leggibile
#define MAX_CLIENT_ACTIONS 100  // numero massimo di azioni inviabili da un client
#define MAX_SOCKETS        10   // numero massimo di associazioni che l'array di associazione socketname-fd contiene

#define checkStop(condizione, messaggio) if(condizione) { fprintf(stderr, "Errore %s (codice %d) | File %s Riga %d (%s)\n", strerror(errno), errno, __FILE__, __LINE__, messaggio); fflush(stderr); exit(EXIT_FAILURE); }
#define checkNull(condizione, messaggio) if(condizione) { fprintf(stderr, "Errore %s (codice %d) | File %s Riga %d (%s)\n", strerror(errno), errno, __FILE__, __LINE__, messaggio); fflush(stderr); return NULL; }
#define checkM1(condizione, messaggio) if(condizione) { fprintf(stderr, "Errore %s (codice %d) | File %s Riga %d (%s)\n", strerror(errno), errno, __FILE__, __LINE__, messaggio); fflush(stderr); return -1; }

#ifdef DEBUG
    #define printSave(f, ...) fprintf(stdout, f, ##__VA_ARGS__); fprintf(stdout, "\n"); fflush(stdout); if(fileLog != NULL) { locka(mutexFileLog); fprintf(fileLog, f, ##__VA_ARGS__); fprintf(fileLog, "\n"); fflush(fileLog); unlocka(mutexFileLog); }
#else
    #define printSave(f, ...) if(fileLog != NULL) { locka(mutexFileLog); fprintf(fileLog, f, ##__VA_ARGS__); fprintf(fileLog, "\n"); fflush(fileLog); unlocka(mutexFileLog); }
#endif

#define locka(mutex) if(pthread_mutex_lock(&mutex) != 0) { fprintf(stderr, "Lock semaforo fallita | File %s Riga %d\n", __FILE__, __LINE__); exit(EXIT_FAILURE); }
#define unlocka(mutex) if(pthread_mutex_unlock(&mutex) != 0) { fprintf(stderr, "Unlock semaforo fallita | File %s Riga %d\n", __FILE__, __LINE__); exit(EXIT_FAILURE); }


/**
 * @brief Allocazione di memoria che blocca tutto in caso di problemi e che imposta tutto a 0.
 * 
 * @param size  Dimensione da allocare in bytes.
**/
void* cmalloc(size_t size);


/**
 * @brief Dà un valore in base a se il carattere specificato è nella stringa o no.
 * 
 * @param ago       simbolo da cercare
 * @param pagliaio  stringa in cui cercare
 * 
 * @return 0 se il simbolo è trovato, -1 altrimenti
**/
int containsCharacter(char ago, char* pagliaio);


/**
 * @brief Stampa l'inizio del colore.
 * 
 * @param num codice colore
**/
void ppf(int num);


/**
 * @brief Stampa fine del colore.
**/
void ppff(void);


/**
 * @brief Stampa un errore.
 * 
 * @param string    stringa da stampare
**/
void pe(char* string);


/**
 * @brief Controlla se la differenza di 2 timespec è maggiore o uguale a quella di una terza timespec.
 * 
 * @param tempoInizio   primo timespec da controllare
 * @param tempoFine     secondo timespec da controllare
 * @param differenza    terzo timespec che deve essere minore della differenza tra i due
 * 
 * @return 1 se la differenza è maggiore di differenza, 0 altrimenti
**/
struct timespec; // altrimenti dà warning
int timespecDiff(struct timespec tempoInizio, struct timespec tempoFine, struct timespec differenza);


/**
 * @brief Compara 2 stringhe, indifferentemente dal fatto che abbiano una nuova riga o un NULL alla fine.
 * 
 * @param s1    Stringa 1
 * @param s2    Stringa 2
 * 
 * @returns 0 se sono le due stringhe sono uguali, -1 altrimenti
**/
int strcmpnl(const char *s1, const char *s2);

#endif /* UTILS_H */