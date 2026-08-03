#ifndef COMMANDS_H
#define COMMANDS_H

#include "client.h"
#include "resp.h"

typedef struct {
    const char *name;
    size_t name_len;
    void (*handler)(client_t *client);
} commands_t;

void cmd_unknown(client_t *client);
void cmd_ping(client_t *client);
void dispatch(client_t *client);
#endif
