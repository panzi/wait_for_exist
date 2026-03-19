#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "test.h"
#include "tests.h"
#include "test_utils.h"
#include "test_utils.h"

#include "normpath.h"

char *binary_path = NULL;
char *valgrind_path = NULL;
bool use_valgrind = false;

char *which(const char *command) {
    if (command == NULL) {
        errno = EINVAL;
        return NULL;
    }

    if (*command == '/') {
        if (access(command, R_OK | X_OK) != 0) {
            return NULL;
        }
        return strdup(command);
    }

    if (strchr(command, '/') != NULL) {
        char *command_path = normpath(command);
        if (command_path != NULL) {
            if (access(command_path, R_OK | X_OK) != 0) {
                free(command_path);
                return NULL;
            }
        }
        return command_path;
    }

    const char *path = getenv("PATH");
    if (path == NULL) {
        errno = EINVAL;
        return NULL;
    }

    long path_max = sysconf(_PC_PATH_MAX);
    if (path_max <= 0) {
        return NULL;
    }

    size_t command_len = strlen(command);
    if (command_len >= path_max) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    char *buf = malloc(path_max + 1);
    if (buf == NULL) {
        return NULL;
    }
    const char *ptr = path;

    while (*ptr) {
        const char *nextptr = strchr(ptr, ':');
        size_t len = nextptr == NULL ? strlen(ptr) : nextptr - ptr;
        if (len > (size_t)path_max - 1 - command_len) {
            continue;
        }
        memcpy(buf, ptr, len);
        buf[len ++] = '/';
        memcpy(buf + len, command, command_len);
        len += command_len;
        buf[len ++] = 0;

        char *command_path = normpath(buf);
        if (command_path == NULL) {
            free(buf);
            return NULL;
        }

        if (access(command_path, R_OK | X_OK) == 0) {
            free(buf);
            return command_path;
        }

        free(command_path);

        if (!nextptr) break;
        ptr = nextptr + 1;
    }

    free(buf);

    errno = ENOENT;
    return NULL;
}

int main(int argc, char *argv[]) {
    binary_path = canonicalize_file_name(BINARY_PATH);

    if (binary_path == NULL) {
        fprintf(stderr, "canonicalize_file_name(\"%s\"): %s", BINARY_PATH, strerror(errno));
        return 1;
    }

    const char *use_valgrind_env = getenv("USE_VALGRIND");
    use_valgrind = use_valgrind_env != NULL && (strcmp(use_valgrind_env, "1") || strcasecmp(use_valgrind_env, "true"));

    if (use_valgrind) {
        valgrind_path = which("valgrind");

        if (valgrind_path == NULL) {
            fprintf(stderr, "which(\"valgrind\"): %s", strerror(errno));
            return 1;
        }
    }

    int res = test_main(argc, argv, (Test[]){
        GET_TEST(normpath_empty),
        GET_TEST(normpath_simple),
        GET_TEST(normpath_absolute),
        GET_TEST(normpath_double_slash),
        GET_TEST(normpath_trailing_slash),
        GET_TEST(normpath_dot),
        GET_TEST(normpath_double_dot),
        GET_TEST(normpath_everything),
        GET_TEST(path_existing),
        GET_TEST(parent_exists),
        GET_TEST(deep_path),
        GET_TEST(create_and_delete),
        { NULL, NULL },
    });

    free(binary_path);
    binary_path = NULL;

    free(valgrind_path);
    valgrind_path = NULL;

    return res;
}
