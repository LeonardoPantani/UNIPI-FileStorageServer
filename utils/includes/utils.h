#define CLR_WARNING 93
#define CLR_SUCCESS 92
#define CLR_ERROR 91
#define CLR_INFO 90
#define CLR_DEFAULT 0

#define MAX_LINE_SIZE 1000

enum ErroriConfig {
    ERR_FILEOPENING = -1,
    ERR_INVALIDKEY  = -2,
    ERR_NEGVALUE    = -3,
    ERR_EMPTYVALUE  = -4,
    ERR_UNSETVALUES = -5,
    ERR_ILLEGAL     = -6,
};

int containsCharacter(char symbol, char* string) {
    for(int i = 0; i < strlen(string); i++) {
        if(string[i] == symbol)
            return 1;
    }
    return 0;
}

void ppf(int num) { // stampa inizio colore
    printf("\033[%dm", num);
}

void ppff() { // stampa fine colore
    printf("\033[0m");
}

void pp(char* string, int num) {
    printf("\033[%dm%s\033[0m\n", num, string);
}

void pe(char* string) {
    printf("\033[91m%s: %s (codice %d)\033[0m\n", string, strerror(errno), errno);
}