#define FALSE 0
#define TRUE 1

#define checkStop(condizione, messaggio) if(condizione) { fprintf(stderr, "Errore %s (codice %d) | File %s Riga %d", strerror(errno), errno, __FILE__, __LINE__); fflush(stderr); exit(EXIT_FAILURE); }
#define checkNull(condizione, messaggio) if(condizione) { fprintf(stderr, "Errore %s (codice %d) | File %s Riga %d", strerror(errno), errno, __FILE__, __LINE__); fflush(stderr); return NULL; }
#define checkM1(condizione, messaggio) if(condizione) { fprintf(stderr, "Errore %s (codice %d) | File %s Riga %d", strerror(errno), errno, __FILE__, __LINE__); fflush(stderr); return -1; }

#ifdef DEBUG
    #define stampaDebug(testo) if(DEBUG) { fprintf(stderr, "%s\n", testo); fflush(stderr); } else { }
#else
    #define stampaDebug(testo) {}
#endif

#define locka(semaforo) if(pthread_mutex_lock(&semaforo) != 0) { fprintf(stderr, "Lock semaforo fallita | File %s Riga %d", __FILE__, __LINE__); exit(EXIT_FAILURE); }
#define unlocka(semaforo) if(pthread_mutex_unlock(&semaforo) != 0) { fprintf(stderr, "Unlock semaforo fallita | File %s Riga %d", __FILE__, __LINE__); exit(EXIT_FAILURE); }