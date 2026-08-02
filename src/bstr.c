#include "bstr.h"
#include <stdbool.h>
#include <stddef.h>

bool bstr_eq(const bstr_t *a, const bstr_t *b) {
    if (a->len != b->len) {
        return false;
    }
    size_t len = a->len;
    for (size_t i = 0; i < len; i++) {
        if (a->data[i] != b->data[i]) {
            return false;
        }
    }
    return true;
}

bool bstr_eq_cstr(const bstr_t *b, const char *c, size_t c_len) {
    if (c_len != b->len) {
        return false;
    }
    bstr_t tmp = {.len = c_len, .data = (char *)c};
    return bstr_eq(b, &tmp);
}