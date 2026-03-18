#ifndef TEST_UTILS_H
#define TEST_UTILS_H
#pragma once

#define _GNU_SOURCE
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <spawn.h>
#include <sys/wait.h>

#ifdef __cplusplus
extern "C" {
#endif

void make_dir(const char *suffix);
void make_file(const char *suffix);
void rm(const char *suffix);
struct timespec timespec_sub(const struct timespec *lhs, const struct timespec *rhs);

typedef struct ProcInfo {
    char *filename;
    struct timespec started_at;
    pid_t pid;
} ProcInfo;

#ifndef BINARY_PATH
#define BINARY_PATH "./build/debug/wait_for_exist"
#endif

extern char *binary_path;
extern char *valgrind_path;
extern bool use_valgrind;

ProcInfo spawn_wait_for_exist(const char *suffix);

#define TEST_TIMEOUT 5

#define assert_proc_ok(PROC) { \
    alarm(TEST_TIMEOUT); /* HACK: using alarm() for timeout */ \
    int _test_status = 0; \
    int _test_wait_res = waitpid((PROC)->pid, &_test_status, 0); \
    \
    struct timespec _test_ended_at = { .tv_sec = 0, .tv_nsec = 0 }; \
    clock_gettime(CLOCK_MONOTONIC, &_test_ended_at); \
    \
    if (_test_wait_res == -1) { \
        int errnum = errno; \
        if (errnum == EINTR && timespec_sub(&_test_ended_at, &(PROC)->started_at).tv_sec >= TEST_TIMEOUT) { \
            test_assertf(false, "timeout running %s %s", BINARY_PATH, (PROC)->filename); \
        } else { \
            test_assertf(_test_wait_res >= 0, "error wating for %s %s: %s", BINARY_PATH, (PROC)->filename, strerror(errnum)); \
        } \
    } else if (WIFSIGNALED(_test_status)) { \
        test_assertf(false, "process %s %s terminated by signal %d", BINARY_PATH, (PROC)->filename, WTERMSIG(_test_status)); \
    } else if (WIFSTOPPED(_test_status)) { \
        test_assertf(false, "process %s %s stopped by signal %d", BINARY_PATH, (PROC)->filename, WSTOPSIG(_test_status)); \
    } else if (WIFEXITED(_test_status)) { \
        int _test_exit_status = WEXITSTATUS(_test_status); \
        test_assertf(_test_exit_status == 0, "process %s %s exit status: %d", BINARY_PATH, (PROC)->filename, _test_exit_status); \
    } else { \
        test_assertf(_test_status == 0, "process %s %s status: %d", BINARY_PATH, (PROC)->filename, _test_status); \
    } \
    sleep(0); /* HACK: clear alarm */ \
}

#define assert_proc_running(PROC) { \
    int _test_proc_status = 0; \
    test_assertf(waitpid((PROC)->pid, &_test_proc_status, WNOHANG) == 0, "process isn't running %s %s", BINARY_PATH, (PROC)->filename); \
}

#ifdef __cplusplus
}
#endif

#endif
