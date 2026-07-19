#ifndef BSTR_H
#define BSTR_H

typedef struct {
    size_t len;
    char *data;
} bstr_t;

#endif