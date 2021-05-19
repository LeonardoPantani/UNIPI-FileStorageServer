/**
 * @file    actions.h
 * @brief   Contiene i codici delle azioni delle richieste e risposta del server
**/

typedef enum {
    /**
     * SERVER
    **/
   AC_WELCOME = 0,
   AC_CLOSING = 1,

    /**
     * CLIENT
    **/
   AC_HELLO   = 2,
   AC_STOPPING = 3,

   /**
    * Altro
    **/
   AC_UNKNOWN = 4,
} ActionType;