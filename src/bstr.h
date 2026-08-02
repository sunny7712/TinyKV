#ifndef BSTR_H
#define BSTR_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    size_t len;
    char *data;
} bstr_t;

bool bstr_eq(const bstr_t *a, const bstr_t *b);
bool bstr_eq_cstr(const bstr_t *b, const char *c, size_t c_len);

#endif