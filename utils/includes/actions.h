/**
 * @file    actions.h
 * @brief   Contiene i codici delle azioni delle richieste e risposta del server
**/

typedef enum {
    /**
     * SERVER
    **/
   AC_WELCOME = 0,
   AC_STOPPING = 1,
   AC_CANTDO = 11,
   AC_FILEOPENED = 12,
   AC_OPEN = 13,
   AC_FILEEXISTS = 14,
   AC_FILENOTEXISTS = 15,
   AC_MAXCONNECTIONSREACHED = 16,
   AC_FILESVD = 17,
   AC_FLUSH_START = 19,
   AC_FLUSHEDFILE = 20,
   AC_FLUSH_END = 21,
   AC_READ = 22,
   AC_FILERCVD = 23,
   AC_READ_MULTIPLE = 24,
   AC_START_SEND = 25,
   AC_FILE_SENT = 26,
   AC_FINISH_SEND = 27,
   AC_NOTFORU = 28,
   AC_CLOSE = 29,
   AC_CLOSED = 30,
   AC_DELETED = 31,
   AC_WRITE = 32,
   AC_FILENOTNEW = 33,
   AC_APPEND = 34,
   AC_LOCK = 35,
   AC_UNLOCK = 36,
   AC_LOCKED = 37,
   AC_UNLOCKED = 38,
   AC_BADRQST = 39,

    /**
     * CLIENT
    **/
   AC_HELLO   = 2,
   AC_WRITE_RECU = 4,
   AC_WRITE_LIST = 5,
   AC_READ_LIST = 6,
   AC_READ_RECU = 7,
   AC_ACQUIRE_MUTEX = 8,
   AC_RELEASE_MUTEX = 9,
   AC_DELETE = 10,

   /**
    * Altro
    **/
   AC_UNKNOWN = 50,
} ActionType;