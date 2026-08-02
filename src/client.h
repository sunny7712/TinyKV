#ifndef CLIENT_H
#define CLIENT_H

#include "bstr.h"
#include <stddef.h>

#define OUTBUF_SIZE 8192
#define INBUF_SIZE 8192
#define MAX_ARGS 100

#define MAX_BULK_LEN 4096

typedef enum {
    PARSE_START,      // In this state, till we parse *
    PARSE_ARRAY_LEN,  // Found *, reading digits until \r\n to get args_total
    PARSE_BULK_START, // Found array length, looking for $
    PARSE_BULK_LEN,   // saw $, reading digits until \r\n to get current_len
    PARSE_BULK_DATA   //  know length, reading exactly current_len bytes into
                      //  current arg

} parser_state_t;

typedef struct {
    int fd;
    char outbuf[OUTBUF_SIZE]; // queued bytes waiting to be sent
    size_t out_queued;        // total bytes currently queued in outbuf
    size_t out_sent;          // bytes already sent from outbuf

    char inbuf[INBUF_SIZE];
    size_t inbuf_len;           // number of bytes in inbuf
    size_t inbuf_processed_pos; // index up to which inbuf has been processed

    parser_state_t parser_state;
    int args_total; // total elements in the input array
    int args_parsed;
    size_t current_len;  // current bulk string len
    size_t current_read; // len of current bulk string parsed
    bstr_t *argv;
    int argc;

} client_t;

void client_init(int fd, client_t *client);
void client_free(client_t *client);
void free_argv(client_t *client);
int queue_bytes(client_t *client, const char *data, size_t len);
int compact_inbuf(client_t *client);
int append_inbuf(client_t *client, const char *data, size_t len);

#endif
