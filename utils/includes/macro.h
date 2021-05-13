#define FALSE 0
#define TRUE 1

#define stopProgramma(condizione, messaggio) if(condizione) { fprintf(stderr, "Errore %s (codice %d) | File %s Riga %d", strerror(errno), errno, __FILE__, __LINE__); fflush(stderr); exit(EXIT_FAILURE); }