#ifndef RESP_H
#define RESP_H

#include "client.h"

typedef enum {
    PARSE_OK,
    PARSE_NEED_MORE,
    PARSE_ERR
} parse_status_t;

parse_status_t parse_inbuf(client_t *client);

#endif
