#ifndef STRBUF_H
#define STRBUF_H
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct StrBuf {
    char *data;
    size_t capacity;
    size_t used;
} StrBuf;

#define STRBUF_INIT { .data = NULL, .capacity = 0, .used = 0 }

int strbuf_append(StrBuf *buf, const char *data, size_t size);
int strbuf_append_str(StrBuf *buf, const char *data);
void strbuf_destroy(StrBuf *buf);


#ifdef __cplusplus
}
#endif

#endif
