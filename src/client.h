#ifndef CLIENT_H
#define CLIENT_H

#include <stddef.h>

#define OUTBUF_SIZE 8192
#define INBUF_SIZE 8192

typedef enum {
    PARSE_START,
    PARSE_ARRAY_LEN,
    PARSE_BULK_STR

} parser_state_t;

typedef struct Client {
    int fd;
    char outbuf[OUTBUF_SIZE]; // queued bytes waiting to be sent
    size_t out_queued;        // total bytes currently queued in outbuf
    size_t out_sent;          // bytes already sent from outbuf

    char inbuf[INBUF_SIZE];
    size_t inbuf_len;
    size_t inbuf_processed_pos;
    char **parsed_commands;

    parser_state_t parser_state;
    size_t args_total;
    size_t args_parsed;
    size_t current_len;
    size_t current_read;

} client_t;

#endif