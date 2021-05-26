/**
 * @file    message.c
 * @brief   Contiene l'implementazione dei tipi dei messaggi e alcune funzioni utili per facilitare l'utilizzo.
**/

#include "message.h"

static void setMessage(Message* msg, ActionType ac, int flags, char* path, void* data, size_t data_length) {
    msg->action = ac;

    if(path != NULL) {
        msg->path_length = strlen(path);
    } else {
        msg->path_length = 0;
    }
    msg->path = path;

    msg->data_length = data_length;
    msg->data = data;

    char* a = data;
}