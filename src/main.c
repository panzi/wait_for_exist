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
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <getopt.h>

#include "wait_for_exist.h"
#include "normpath.h"

static void print_usage(int argc, char *const argv[]) {
    const char *progname = argc > 0 ? argv[0] : "wait_for_exist";

    printf("%s: [--help] [--version] [--timeout=SECONDS] <path>\n", progname);
}

static void print_help(int argc, char *const argv[]) {
    print_usage(argc, argv);
}

static const char *skipws(const char *source) {
    if (isspace(*source)) ++ source;
    return source;
}

static const char *parse_uint64(const char *source, uint64_t *output) {
    assert(output != NULL);
    uint64_t res = 0;

    const char *ptr = source;
    while (*ptr >= '0' && *ptr <= '9') {
        uint64_t value = *ptr - '0';
        if (res > UINT64_MAX / 10) {
            errno = ERANGE;
            return NULL;
        }
        res *= 10;

        if (res > UINT64_MAX - value) {
            errno = ERANGE;
            return NULL;
        }
        res += value;
        ++ ptr;
    }

    if (ptr == source) {
        errno = EINVAL;
        return NULL;
    }

    *output = res;

    return ptr;
}

static int parse_timespec(const char *source, struct timespec *timespec) {
    uint64_t sec = 0;
    uint64_t nsec = 0;

    const char *ptr = skipws(source);

    ptr = parse_uint64(ptr, &sec);
    if (ptr == NULL) return -1;

    if (*ptr == '.') {
        ++ ptr;
        const char *next = parse_uint64(ptr, &nsec);
        if (next == NULL) {
            return -1;
        }

        size_t frac_len = next - ptr;
        if (frac_len > 9) {
            while (frac_len > 9) {
                nsec /= 10;
                -- frac_len;
            }
        } else if (frac_len < 9) {
            while (frac_len < 9) {
                nsec *= 10;
                ++ frac_len;
            }
        }

        ptr = next;
    }

    ptr = skipws(ptr);

    if (*ptr) {
        errno = EINVAL;
        return -1;
    }

    assert(timespec != NULL);
    timespec->tv_sec = sec;
    timespec->tv_nsec = nsec;

    return 0;
}

int main(int argc, char* argv[]) {
#ifdef NORMPATH_MAIN
    for (size_t index = 1; index < argc; ++ index) {
        char *path = normpath(argv[index]);
        if (path == NULL) {
            fprintf(stderr, "%s: %s\n", argv[index], strerror(errno));
        }
        printf("%s -> %s\n", argv[index], path);
        free(path);
    }
#else
    struct timespec timeout = { .tv_sec = 0, .tv_nsec = 0 };
    bool has_timeout = false;

    static const struct option long_options[] = {
        {"timeout", required_argument, 0,  't' },
        {"help",    no_argument,       0,  'h' },
        {"version", no_argument,       0,  'v' },
        {0,         0,                 0,   0  },
    };

    for (;;) {
        int opt = getopt_long(argc, argv, "t:hv", long_options, NULL);

        if (opt == -1)
            break;

        switch (opt) {
            case 'h':
                print_help(argc, argv);
                return 0;

            case 'v':
                printf("%s\n", WAIT_FOR_EXIST_VERSION);
                return 0;

            case 't':
                if (parse_timespec(optarg, &timeout) != 0) {
                    fprintf(stderr, "illegal value for option --timeout=%s: %s\n", optarg, strerror(errno));
                    return 1;
                }
                has_timeout = true;
                break;

            case '?':
                return 1;
        }
    }

    int actual_argc = argc - optind;
    if (actual_argc != 1) {
        print_usage(argc, argv);
        return 1;
    }

    const char *path = argv[optind];
    int errnum = wait_for_exist(path, has_timeout ? &timeout : NULL);

    if (errnum != 0) {
        fprintf(stderr, "%s: %s\n", path, strerror(errnum));
        return 1;
    }
#endif

    return 0;
}
