/**
 * @file    actions.h
 * @brief   Contiene i codici delle azioni delle richieste e risposta del server.
 * @author  Leonardo Pantani
**/

typedef enum {
    /**
     *  RISPOSTE SERVER -> CLIENT
    **/
    // generiche
    ANS_UNKNOWN             = 0,
    ANS_NO_PERMISSION       = 1,
    ANS_MAX_CONN_REACHED    = 2,
    ANS_OK                  = 3,
    ANS_BAD_RQST            = 4,

    // handshake server-client
    ANS_WELCOME             = 5,
    ANS_HELLO               = 6,

    // relative a file
    ANS_FILE_EXISTS         = 7,
    ANS_FILE_NOT_EXISTS     = 8,
    ANS_STREAM_START        = 9,
    ANS_STREAM_FILE         = 10,
    ANS_STREAM_END          = 11,


    /**
     * RICHIESTE CLIENT -> SERVER
    */
    REQ_OPEN                = 12,
    REQ_READ                = 13,
    REQ_READ_N              = 14,
    REQ_WRITE               = 15,
    REQ_APPEND              = 16,
    REQ_LOCK                = 17,
    REQ_UNLOCK              = 18,
    REQ_CLOSE               = 19,
    REQ_DELETE              = 20,


    /**
     * AZIONI DEL CLIENT (servono per identificare l'operazione da svolgere)
    **/
    AC_WRITE_RECU           = 1, // -w
    AC_WRITE_LIST           = 2, // -W
    AC_READ_LIST            = 3, // -r
    AC_READ_RECU            = 4, // -R
    AC_ACQUIRE_MUTEX        = 5, // -l
    AC_RELEASE_MUTEX        = 6, // -u
    AC_DELETE               = 7, // -c
} ActionType;