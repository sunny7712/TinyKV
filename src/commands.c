#include "commands.h"

#define CMD(str, fn) {str, sizeof(str) - 1, fn}

void cmd_unknown(client_t *client) {
    char buf[64];
    int n = encode_error(buf, sizeof(buf), "unknown command");
    if (n > 0) {
        queue_bytes(client, buf, (size_t)n);
    }
}

void cmd_ping(client_t *client) { queue_bytes(client, "+PONG\r\n", 7); }

static const commands_t commands[] = {CMD("PING", cmd_ping)};

void dispatch(client_t *client) {
    if (client->argc == 0) {
        return;
    }

    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if (bstr_eq_cstr(&client->argv[0], commands[i].name,
                         commands[i].name_len)) {
            commands[i].handler(client);
            return;
        }
    }
    cmd_unknown(client);
}
