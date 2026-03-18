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
#include "wait_for_exist.h"
#include "normpath.h"

#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_EVENTS 64

#ifdef NDEBUG
#define DEBUGF(FMT, ...) (void)0;
#else
#define DEBUGF(FMT, ...) fprintf(stderr, "%s:%d:%s: " FMT "\n", __FILE__, __LINE__, __FUNCTION__ __VA_OPT__(,) __VA_ARGS__);
#endif

typedef union {
    char buffer[4096];
    struct inotify_event event;
} EventBuf;

static size_t find_parent_sep(const char *path) {
    size_t index = strlen(path);

    if (index == 0) {
        return 0;
    }

    // select last char
    -- index;

    // trim trailing '/'
    while (path[index] == '/') {
        if (index == 0) {
            return 0;
        }
        -- index;
    }

    // trim path component
    while (path[index] != '/') {
        assert(index > 0);
        if (index == 0) {
            return 0;
        }
        -- index;
    }

    // trim '/' before path component
    while (path[index] == '/') {
        if (index == 0) {
            return 1;
        }
        -- index;
    }

    // index is now before '/', we want to point to the '/'
    return index + 1;
}

int wait_for_exist(const char *path, const struct timespec *timeout) {
    if (path == NULL) {
        DEBUGF("path may not be NULL!");
        return EINVAL;
    }

    EventBuf event_buf;
    struct epoll_event events[MAX_EVENTS];

    char *path_buf = normpath(path);

    int epoll = -1;
    int inotify = -1;
    int wd = -1;
    int status = 0;

    if (path_buf == NULL) {
        int errnum = errno;
        DEBUGF("normpath(\"%s\"): %s", path, strerror(errnum));
        return errnum == 0 ? ENOMEM : errnum;
    }

    const size_t path_len = strlen(path_buf);

    inotify = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);

    if (inotify < 0) {
        status = errno;
        DEBUGF("inotify_init1(IN_CLOEXEC | IN_NONBLOCK): %s", strerror(status));
        goto error;
    }

    epoll = epoll_create1(EPOLL_CLOEXEC);

    if (epoll < 0) {
        status = errno;
        DEBUGF("epoll_create1(EPOLL_CLOEXEC): %s", strerror(status));
        goto error;
    }

    if (epoll_ctl(epoll, EPOLL_CTL_ADD, inotify, &(struct epoll_event){
        .events = EPOLLIN,
        .data.fd = inotify,
    }) != 0) {
        status = errno;
        DEBUGF("epoll_ctl(%d, EPOLL_CTL_ADD, %d, &event): %s", epoll, inotify, strerror(status));
        goto error;
    }

    const char *ptr = event_buf.buffer;
    const char *endptr = event_buf.buffer;
    int event_bytes = 0;

    for (;;) {
        if (wd >= 0) {
            if (inotify_rm_watch(inotify, wd) != 0) {
                DEBUGF("inotify_rm_watch(%d, %d): %s", inotify, wd, strerror(errno));
            }
            wd = -1;
        }

        size_t sep_index = find_parent_sep(path_buf);
        if (sep_index == 0) {
            // already at root!
            status = EINVAL;
            DEBUGF("Filesystem root is missing?");
            goto error;
        }
        path_buf[sep_index] = 0;

        wd = inotify_add_watch(inotify, path_buf, IN_CREATE | IN_MOVED_TO | IN_DELETE_SELF | IN_MOVE_SELF);

        if (wd < 0) {
            int errnum = errno;
            if (errnum == ENOENT) {
                goto parent;
            } else {
                status = errnum;
                DEBUGF("inotify_add_watch(%d, \"%s\", IN_CREATE | IN_MOVED_TO | IN_DELETE_SELF | IN_MOVE_SELF): %s", inotify, path_buf, strerror(status));
                goto error;
            }
        }

        path_buf[sep_index] = '/';

        if (access(path_buf, F_OK) != 0) {
            int errnum = errno;
            if (errnum != ENOENT) {
                status = errnum;
                DEBUGF("access(\"%s\", F_OK): %s", path_buf, strerror(status));
                goto error;
            }
        } else {
            goto child;
        }

        path_buf[sep_index] = 0;

        for (;;) {
            bool waiting = true;

            while (waiting) {
                int event_count = epoll_pwait2(epoll, events, MAX_EVENTS, timeout, NULL);
                if (event_count < 0) {
                    status = errno;
                    DEBUGF("epoll_pwait2(%d, events, MAX_EVENTS, timeout, NULL): %s", epoll, strerror(status));
                    goto error;
                }

                if (event_count == 0) {
                    status = ETIMEDOUT;
                    DEBUGF("epoll_pwait2(%d, events, MAX_EVENTS, timeout, NULL): %s", epoll, strerror(status));
                    goto error;
                }

                for (int index = 0; index < event_count; ++ index) {
                    struct epoll_event *event = &events[index];
                    if (event->events & EPOLLIN && event->data.fd == inotify) {
                        waiting = false;
                        break;
                    }
                }

                assert(!waiting);
            }

            if (ptr >= endptr) {
                event_bytes = read(inotify, event_buf.buffer, sizeof(event_buf.buffer));

                if (event_bytes == 0) {
                    // shouldn't happen!
                    status = EPIPE;
                    DEBUGF("read(%d, %p, %" PRIuPTR "): end of file", inotify, event_buf.buffer, sizeof(event_buf.buffer));
                    goto error;
                }

                if (event_bytes < 0) {
                    status = errno;
                    DEBUGF("read(%d, %p, %" PRIuPTR "): %s", inotify, event_buf.buffer, sizeof(event_buf.buffer), strerror(status));
                    goto error;
                }

                endptr = event_buf.buffer + event_bytes;
            }

            while (ptr < endptr) {
                struct inotify_event *event = (struct inotify_event*)ptr;
                ptr += sizeof(struct inotify_event) + event->len;

                if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
                    goto child;
                }

                if (event->mask & (IN_DELETE_SELF | IN_MOVE_SELF)) {
                    goto parent;
                }
            }
        }

        assert(false); // should not be reachable
        goto error;

    child:
        path_buf[sep_index] = '/';

        if (strlen(path_buf) == path_len) {
            break;
        }

    parent:
    }

    goto cleanup;

error:
    if (status == 0) {
        status = EINVAL;
    }

cleanup:
    if (inotify >= 0) {
        close(inotify);
        inotify = -1;
    }

    if (epoll >= 0) {
        close(epoll);
        epoll = -1;
    }

    if (path_buf != NULL) {
        free(path_buf);
        path_buf = NULL;
    }

    return status;
}
