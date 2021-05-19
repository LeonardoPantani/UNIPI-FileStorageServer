/**
 * @file    client_queue.h
 * @brief   Contiene l'header dei metodi utilizzati per la coda di client.
**/

typedef struct {
    int first;
    int last;
    int *array;
    int maxClients;
    int numClients;
} ClientQueue;

/**
 * @brief Crea una coda di al massimo numClients.
 * 
 * @param numClients    Numero di posti nella coda
 * 
 * @returns Puntatore alla coda creata, NULL in caso di errore
**/
ClientQueue* createQueue(int numClients);

/**
 * @brief Elimina una coda di client.
 * 
 * @param coda  La coda su cui eseguire la free
**/
void deleteQueue(ClientQueue *coda);

/**
 * @brief Aggiunge un elemento alla coda.
 * 
 * @param coda      La coda a cui aggiungere un elemento
 * @param elemento  L'elemento da aggiungere
 * 
 * @returns 1 in caso di successo, 0 in caso di fallimento
**/
int addToQueue(ClientQueue *coda, int elemento);

/**
 * @brief Rimuove il primo elemento dalla coda
 * 
 * @param coda      La coda da cui rimuovere l'elemento
 * 
 * @returns 1 in caso di successo, 0 in caso di fallimento
**/
int removeFromQueue(ClientQueue *coda);