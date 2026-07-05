#include "client.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>

void client_init(int fd, client_t *client) {
    memset(client, 0, sizeof(*client));
    client->fd = fd;
    // PARSE_START = 0, already zeroed
}

void client_free(client_t *client) {
    if (client->argv) {
        for (int i = 0; i < client->argc; i++) {
            if (client->argv[i]) {
                free(client->argv[i]);
            }
        }
        free(client->argv);
    }
    close(client->fd);
}

int queue_bytes(client_t *client, const char *data, size_t len) {
    // The primary difference between memcpy and memmove is how they handle
    // overlapping memory regions.
    //  memcpy assumes the source and destination memory buffers do not overlap,
    //  resulting in undefined behavior if they do. In contrast, memmove safely
    //  allows overlapping regions by copying the data in a manner that prevents
    //  data corruption.

    if (client->out_sent > 0) {
        if (client->out_sent < client->out_queued) {
            memmove(client->outbuf, client->outbuf + client->out_sent,
                    client->out_queued - client->out_sent);
        }
        client->out_queued = client->out_queued - client->out_sent;
        client->out_sent = 0;
    }

    if (client->out_queued + len > sizeof(client->outbuf)) {
        return -1;
    }

    memcpy(client->outbuf + client->out_queued, data, len);
    client->out_queued += len;
    return 0;
}