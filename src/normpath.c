/**
 * wait_for_exist  Copyright (C) 2026  Mathias Panzenböck
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#define _GNU_SOURCE

#include "normpath.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <inttypes.h>

char *normpath(const char *path) {
    if (path == NULL) {
        errno = EINVAL;
        return NULL;
    }

    const size_t path_len = strlen(path);

    char *buf = NULL;
    size_t used = 0;
    size_t capacity = 0;

    if (path[0] != '/') {
        // releative path
        long path_max = sysconf(_PC_PATH_MAX);
        if (path_max <= 0) {
            return NULL;
        }

        if (path_len >= SIZE_MAX - 2 - (size_t)path_max) {
            // overflow check
            // the 2 is for '/' and '\0'
            errno = ERANGE;
            return NULL;
        }
        capacity = path_len + 2 + path_max;
        buf = malloc(capacity);
        if (buf == NULL) {
            return NULL;
        }

        if (getcwd(buf, capacity) == NULL) {
            free(buf);
            return NULL;
        }

        used = strlen(buf);
        buf[used ++] = '/';

        memcpy(buf + used, path, path_len);
        used += path_len;
    } else {
        buf = strdup(path);
        if (buf == NULL) {
            return NULL;
        }

        used = path_len;
        capacity = used + 1;
    }
    buf[used] = 0;

    assert(used > 0);

    const char *src = buf;
    char *dest = buf;
    const char *endptr = buf + used;

    while (src < endptr) {
        assert(*src == '/');

        while (*(++ src) == '/');

        const char *name_end_ptr = src;
        while (name_end_ptr < endptr && *name_end_ptr != '/') {
            ++ name_end_ptr;
        }

        if (name_end_ptr == src && *name_end_ptr == 0) {
            if (dest == buf) {
                *(dest ++) = '/';
            }
            break;
        }

        *(dest ++) = '/';

        size_t namelen = (size_t)(name_end_ptr - src);

        if (namelen == 1 && *src == '.') {
            // remove any '.'
            if (dest > buf) {
                -- dest;
            }
        } else if (namelen == 2 && src[0] == '.' && src[1] == '.') {
            if ((dest - 1) > buf) {
                char *new_dest = memrchr(buf, '/', (size_t)(dest - buf) - 1);
                assert(new_dest != NULL);

                if (new_dest == NULL || new_dest == buf) {
                    dest = buf + 1;
                } else {
                    dest = new_dest;
                }
            } else if (dest > buf) {
                -- dest;
            }
        } else {
            memcpy(dest, src, namelen);
            dest += namelen;
        }
        src = name_end_ptr;
    }

    if (dest == buf) {
        *dest = '/';
        ++ dest;
    } else {
        // strip trailing '/'
        while (dest > buf && *(dest - 1) == '/') {
            -- dest;
        }
    }

    assert(dest <= endptr);
    *dest = 0;

    const size_t actual_capacity = (size_t)(dest - buf) + 1;
    if (capacity == actual_capacity) {
        return buf;
    }

    assert(actual_capacity > 0);
    char *shrunk = realloc(buf, actual_capacity);

    // GCC thinks `buf` is used after free here, but `buf` is only returned if
    // there was an error in `realloc()`. This is error handling, not an error!
    // clang shows no warnings.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuse-after-free"
    return actual_capacity == 0 || shrunk != NULL ? shrunk : buf;
#pragma GCC diagnostic pop
}
