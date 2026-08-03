#include "resp.h"
#include "bstr.h"
#include "client.h"
#include "commands.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

parse_status_t parse_crlf_terminated_integer(char *buf, size_t *current_pos,
                                             size_t buf_len, int *out_value) {
    if (buf == NULL || out_value == NULL || current_pos == NULL) {
        return PARSE_ERR;
    }
    if ((*current_pos) >= buf_len) {
        return PARSE_NEED_MORE;
    }

    void *ptr = memchr(buf + *current_pos, '\r', buf_len - (*current_pos));
    char *start_ptr = buf + *current_pos;
    char *end_ptr = (char *)ptr;

    if (end_ptr == NULL) {
        return PARSE_NEED_MORE;
    }

    /* We found '\r', but '\n' hasn't arrived yet. */
    if ((size_t)(end_ptr - buf + 1) >= buf_len) {
        return PARSE_NEED_MORE;
    }

    if (*(end_ptr + 1) != '\n') {
        return PARSE_ERR;
    }

    if (!((*start_ptr >= '0' && *start_ptr <= '9') || *start_ptr == '-')) {
        return PARSE_ERR;
    }

    errno = 0;
    char *num_end_ptr = NULL;
    long value = strtol(start_ptr, &num_end_ptr, 10);

    /* No digits were parsed. */
    if (num_end_ptr == start_ptr) {
        return PARSE_ERR;
    }

    /* Parser must stop exactly at '\r'. */
    if (num_end_ptr != end_ptr) {
        return PARSE_ERR;
    }

    /* Overflow/underflow. */
    if (errno == ERANGE) {
        return PARSE_ERR;
    }

    /* Reject negative values. */
    if (value < 0) {
        return PARSE_ERR;
    }

    /* int overflow. */
    if (value > INT_MAX) {
        return PARSE_ERR;
    }

    *out_value = (int)value;
    *current_pos = (size_t)(end_ptr - buf + 2);
    return PARSE_OK;
}

parse_status_t parse_crlf_terminated_string(char *buf, size_t *current_pos,
                                            size_t buf_len, size_t str_len,
                                            char *str) {
    if (buf == NULL || str == NULL || current_pos == NULL) {
        return PARSE_ERR;
    }
    if ((*current_pos) >= buf_len) {
        return PARSE_NEED_MORE;
    }

    char *end_ptr = buf + *current_pos + str_len - 1;

    if ((size_t)(end_ptr - buf + 1) >= buf_len) {
        return PARSE_NEED_MORE;
    }

    if ((size_t)(end_ptr - buf + 2) > buf_len) {
        return PARSE_NEED_MORE;
    } else if (*(end_ptr + 1) != '\r') {
        return PARSE_ERR;
    }

    if ((size_t)(end_ptr - buf + 3) > buf_len) {
        return PARSE_NEED_MORE;
    } else if (*(end_ptr + 2) != '\n') {
        return PARSE_ERR;
    }

    memcpy(str, buf + *current_pos, str_len);
    *current_pos = (size_t)(end_ptr - buf + 3);
    return PARSE_OK;
}

parse_status_t parse_start_of_array(char *buf, size_t *current_pos,
                                    size_t buf_len) {
    if (buf == NULL || current_pos == NULL) {
        return PARSE_ERR;
    }
    if ((*current_pos) >= buf_len) {
        return PARSE_NEED_MORE;
    }

    if (buf[*current_pos] != '*') {
        return PARSE_ERR;
    }

    *current_pos += 1;
    return PARSE_OK;
}

parse_status_t parse_start_of_bulk_string(char *buf, size_t *current_pos,
                                          size_t buf_len) {
    if (buf == NULL || current_pos == NULL) {
        return PARSE_ERR;
    }
    if ((*current_pos) >= buf_len) {
        return PARSE_NEED_MORE;
    }

    if (buf[*current_pos] != '$') {
        return PARSE_ERR;
    }

    *current_pos += 1;
    return PARSE_OK;
}

int encode_error(char *out, size_t out_cap, const char *msg) {
    size_t msg_len = strlen(msg);
    size_t required = 1 + msg_len + 2;

    if (required > out_cap) {
        return -1;
    }

    out[0] = '-';
    memcpy(out + 1, msg, msg_len);
    out[1 + msg_len] = '\r';
    out[1 + msg_len + 1] = '\n';

    return (int)required;
}

parse_status_t parse_inbuf(client_t *client) {
    parse_status_t parse_status = PARSE_OK;

    while (parse_status == PARSE_OK) {

        if (client->parser_state == PARSE_START) {
            parse_status = parse_start_of_array(
                client->inbuf, &client->inbuf_processed_pos, client->inbuf_len);
            if (parse_status == PARSE_OK) {
                client->parser_state = PARSE_ARRAY_LEN;
                continue;
            }
        }

        if (client->parser_state == PARSE_ARRAY_LEN) {
            int out_value;
            parse_status = parse_crlf_terminated_integer(
                client->inbuf, &(client->inbuf_processed_pos),
                client->inbuf_len, &out_value);
            if (parse_status == PARSE_OK) {
                if (out_value == 0) {
                    client->parser_state = PARSE_START;
                    client->args_total = 0;
                    client->args_parsed = 0;
                    // no op
                    free_argv(client);
                    continue;
                }
                if (out_value > MAX_ARGS) {
                    return PARSE_ERR;
                }
                void *ptr = malloc(out_value * sizeof(bstr_t));
                if (ptr == NULL && out_value > 0) {
                    return PARSE_ERR;
                }
                client->argv = (bstr_t *)ptr;
                client->parser_state = PARSE_BULK_START;
                client->args_total = out_value;
                client->args_parsed = 0;
                client->argc = 0;
                continue;
            }
        }

        if (client->parser_state == PARSE_BULK_START) {
            parse_status = parse_start_of_bulk_string(
                client->inbuf, &(client->inbuf_processed_pos),
                client->inbuf_len);
            if (parse_status == PARSE_OK) {
                client->parser_state = PARSE_BULK_LEN;
                continue;
            }
        }

        if (client->parser_state == PARSE_BULK_LEN) {
            int out_value;
            parse_status = parse_crlf_terminated_integer(
                client->inbuf, &(client->inbuf_processed_pos),
                client->inbuf_len, &out_value);
            if (parse_status == PARSE_OK) {
                if (out_value > MAX_BULK_LEN) {
                    return PARSE_ERR;
                }
                void *ptr = malloc((size_t)out_value);
                if (ptr == NULL && out_value > 0) {
                    return PARSE_ERR;
                }
                client->argv[client->args_parsed].data = (char *)ptr;
                client->argv[client->args_parsed].len = out_value;
                client->argc += 1;
                client->parser_state = PARSE_BULK_DATA;
                client->current_len = out_value;
                client->current_read = 0;
                continue;
            }
        }

        if (client->parser_state == PARSE_BULK_DATA) {
            parse_status = parse_crlf_terminated_string(
                client->inbuf, &(client->inbuf_processed_pos),
                client->inbuf_len, client->current_len,
                client->argv[client->args_parsed].data);
            if (parse_status == PARSE_OK) {
                client->args_parsed += 1;
                if (client->args_parsed == client->args_total) {
                    client->parser_state = PARSE_START;
                    client->args_total = 0;
                    client->args_parsed = 0;
                    dispatch(client);

                    free_argv(client);
                } else {
                    client->parser_state = PARSE_BULK_START;
                }
            }
        }
    }
    return parse_status;
}
