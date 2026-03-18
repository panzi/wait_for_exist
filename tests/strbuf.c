#include "strbuf.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>

int strbuf_append(StrBuf *buf, const char *data, size_t size) {
    if (size + 1 > buf->capacity || buf->used > buf->capacity - size - 1) {
        size_t new_capacity = buf->capacity == 0 ? 4096 : buf->capacity * 2;
        if (new_capacity < buf->capacity) {
            errno = ENOMEM;
            return -1;
        }
        char *new_data = realloc(buf->data, new_capacity);
        if (new_data == NULL) {
            return -1;
        }

        buf->data = new_data;
        buf->capacity = new_capacity;
    }

    memcpy(buf->data + buf->used, data, size);
    buf->used += size;
    buf->data[buf->used] = 0;

    return 0;
}

int strbuf_append_str(StrBuf *buf, const char *data) {
    return strbuf_append(buf, data, strlen(data));
}

void strbuf_destroy(StrBuf *buf) {
    free(buf->data);
    buf->data = NULL;
    buf->used = 0,
    buf->capacity = 0;
}
