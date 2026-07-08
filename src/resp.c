#include<string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include "resp.h"

parse_status_t parse_crlf_terminated_integer(char *buf, int current_pos, size_t buf_len, int *out_value, size_t *new_pos) {
    if (buf == NULL || out_value == NULL || new_pos == NULL) {
        return PARSE_ERR;
    }
    if (current_pos < 0) {
        return PARSE_ERR;
    }
    if ((size_t)current_pos >= buf_len) {
        return PARSE_NEED_MORE;
    }

    void *ptr = memchr(buf + current_pos, '\r', buf_len - (size_t) current_pos);
    char *start_ptr = buf + current_pos;
    char *end_ptr = (char *) ptr;

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

    if(!((*start_ptr >= '0' && *start_ptr <= '9') || *start_ptr == '-')) {
        return PARSE_ERR;
    }

    errno = 0;
    char *num_end_ptr = NULL;
    long value = strtol(start_ptr, &num_end_ptr, 10);

    /* No digits were parsed. */
    if(num_end_ptr == start_ptr) {
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
    *new_pos = (size_t)(end_ptr - buf + 2);
    return PARSE_OK;
}
