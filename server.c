#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "utils/includes/macro.h"
#include "utils/includes/utils.h"
#include "utils/includes/config.h"

int main(int argc, char* argv[]) {
    //char* program_name = argv[0];
    Config config;

    pp("---- INIZIO SERVER ----", CLR_INFO);
    pp("> Caricamento file di config", CLR_INFO);
    switch(configLoader(config)) {
        case ERR_FILEOPENING:
            pp("Errore durante l'apertura del file di config.", CLR_ERROR);
            exit(EXIT_FAILURE);
        break;

        case ERR_INVALIDKEY:
            pp("Chiave del server non valida.", CLR_ERROR);
            exit(EXIT_FAILURE);
        break;

        case ERR_NEGVALUE:
            pp("Una o più variabili hanno un valore negativo.", CLR_ERROR);
            exit(EXIT_FAILURE);
        break;

        case ERR_EMPTYVALUE:
            pp("Una o più variabili hanno un valore vuoto.", CLR_ERROR);
            exit(EXIT_FAILURE);
        break;

        case ERR_UNSETVALUES:
            pp("Una o più variabili non sono presenti nel file di config.", CLR_ERROR);
            exit(EXIT_FAILURE);
        break;

        case ERR_ILLEGAL:
            pp("Testo illegale nel file di config.", CLR_ERROR);
            exit(EXIT_FAILURE);
        break;
    }
    pp("> Caricamento config completato.", CLR_INFO);



    // TODO
    return 0;
}