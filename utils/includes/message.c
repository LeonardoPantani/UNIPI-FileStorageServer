/**
 * @file    message.c
 * @brief   Contiene l'implementazione dei tipi dei messaggi e alcune funzioni utili per facilitare l'utilizzo.
**/

#include "message.h"

static void setMessageHeader(Message* msg, ActionType ac, char* path) {
    msg->hdr.action   = ac;
    msg->hdr.abs_path = path;
}

static void setMessageBody(Message* msg, int bdy_length, char* buffer) {
    msg->bdy.length = bdy_length;
    msg->bdy.buffer = buffer;
}