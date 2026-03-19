#define _GNU_SOURCE
#include "normpath.h"
#include "test.h"
#include "test_utils.h"

#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <spawn.h>
#include <stddef.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>

void _make_dir(const char *suffix, const char *expr, const char *src_filename, const char *func_name, size_t lineno) {
    ++ _test_state.current_result->assert_count;

    char filename[4096];
    int res = suffix == NULL ?
        snprintf(filename, sizeof(filename), "/tmp/test.wait_for_exist.%d", getpid()) :
        snprintf(filename, sizeof(filename), "/tmp/test.wait_for_exist.%d/%s", getpid(), suffix);

    if (res <= 0 || res >= sizeof(filename)) {
        _test_fail(
            expr, src_filename, func_name, lineno,
            "create directory name: %s", strerror(errno)
        );
    }

    if (mkdir(filename, 0750) != 0 && errno != EEXIST) {
        _test_fail(
            expr, src_filename, func_name, lineno,
            "error creating directory %s: %s", filename, strerror(errno)
        );
    }
}

void _make_file(const char *suffix, const char *expr, const char *src_filename, const char *func_name, size_t lineno) {
    ++ _test_state.current_result->assert_count;

    char filename[4096];
    int res = snprintf(filename, sizeof(filename), "/tmp/test.wait_for_exist.%d/%s", getpid(), suffix);

    if (res <= 0 || res >= sizeof(filename)) {
        _test_fail(
            expr, src_filename, func_name, lineno,
            "create file name: %s", strerror(errno)
        );
    }

    int fd = open(filename, O_CREAT | O_WRONLY | O_CLOEXEC, 0640);
    int errnum = errno;
    close(fd);

    if (fd < 0) {
        _test_fail(
            expr, src_filename, func_name, lineno,
            "error creating file %s: %s", filename, strerror(errnum)
        );
    }
}

void _rm(const char *suffix, const char *expr, const char *src_filename, const char *func_name, size_t lineno) {
    ++ _test_state.current_result->assert_count;

    char filename[4096];
    int res = snprintf(filename, sizeof(filename), "/tmp/test.wait_for_exist.%d/%s", getpid(), suffix);

    if (res <= 0 || res >= sizeof(filename)) {
        _test_fail(
            expr, src_filename, func_name, lineno,
            "create file name: %s", strerror(errno)
        );
    }

    if (remove(filename) != 0) {
        _test_fail(
            expr, src_filename, func_name, lineno,
            "error deleting %s: %s", filename, strerror(errno)
        );
    }
}

static void _proc_destroy(void *ptr) {
    proc_destroy((ProcInfo*)ptr);
}

ProcInfo *_spawn_wait_for_exist(const char *suffix, const char *expr, const char *src_filename, const char *func_name, size_t lineno) {
    ++ _test_state.current_result->assert_count;

    char buf[4096];

    int res = snprintf(buf, sizeof(buf), "/tmp/test.wait_for_exist.%d/%s", getpid(), suffix);
    if (res <= 0 || res >= sizeof(buf)) {
        _test_fail(
            expr, src_filename, func_name, lineno,
            "create file name: %s", strerror(errno)
        );
    }

    char *filename = strdup(buf);
    if (filename == NULL) {
        _test_fail(
            expr, src_filename, func_name, lineno,
            "strdup(\"%s\"): %s", buf, strerror(errno)
        );
    }

    ProcInfo *proc = malloc(sizeof(ProcInfo));
    if (proc == NULL) {
        free(filename);
        _test_fail(
            expr, src_filename, func_name, lineno,
            "malloc(sizeof(ProcInfo)): %s", strerror(errno)
        );
    }

    proc->filename = filename;
    proc->pid = -1;
    proc->started_at = (struct timespec) {
        .tv_sec = 0,
        .tv_nsec = 0,
    };

    test_cleanup(_proc_destroy, proc);

    clock_gettime(CLOCK_MONOTONIC, &proc->started_at);

    if (use_valgrind) {
        const char*const args[] = {
            "valgrind",
            "--leak-check=yes",
            "--show-leak-kinds=all",
            "--error-exitcode=-1",
            "-s",
            binary_path,
            filename,
            NULL,
        };

        res = posix_spawn(&proc->pid, valgrind_path, NULL, NULL, (char *const*)args, NULL);
    } else {
        const char*const args[] = {
            BINARY_PATH,
            filename,
            NULL,
        };

        res = posix_spawn(&proc->pid, binary_path, NULL, NULL, (char *const*)args, NULL);
    }

    if (res != 0) {
        _test_fail(
            expr, src_filename, func_name, lineno,
        "error spawning process: %s %s: %s", BINARY_PATH, filename, strerror(errno)
        );
    }

    return proc;
}

void proc_destroy(ProcInfo *proc) {
    if (proc->pid > 0) {
        if (kill(proc->pid, SIGTERM) != 0) {
            int errnum = errno;
            if (errnum != ESRCH) {
                fprintf(stderr, "error terminating process %s %s: %s", BINARY_PATH, proc->filename, strerror(errnum));
            }
        } else {
            int status = 0;
            bool ok = false;
            for (int i = 0; i < 5; ++ i) {
                pid_t res = waitpid(proc->pid, &status, WNOHANG);
                if (res > 0) {
                    ok = true;
                    break;
                }
                if (res < 0) {
                    fprintf(stderr, "error waiting for process %s %s: %s", BINARY_PATH, proc->filename, strerror(errno));
                    break;
                }
                usleep(250000);
            }
            if (!ok && (kill(proc->pid, SIGKILL) != 0)) {
                int errnum = errno;
                if (errnum != ESRCH) {
                    fprintf(stderr, "error killing process %s %s: %s", BINARY_PATH, proc->filename, strerror(errnum));
                }
            }
        }
    }
    free(proc->filename);
    proc->filename = NULL;
    proc->pid = -1;
    free(proc);
}

struct timespec timespec_sub(const struct timespec *lhs, const struct timespec *rhs) {
    struct timespec res;
    if (lhs->tv_nsec < rhs->tv_nsec) {
        res.tv_sec  = lhs->tv_sec  - rhs->tv_sec  - 1;
        res.tv_nsec = lhs->tv_nsec - rhs->tv_nsec + 1000000000;
    } else {
        res.tv_sec  = lhs->tv_sec  - rhs->tv_sec;
        res.tv_nsec = lhs->tv_nsec - rhs->tv_nsec;
    }
    return res;
}

#define TEST_TIMEOUT 3

void _assert_proc_ok(const ProcInfo *proc, const char *expr, const char *filename, const char *func_name, size_t lineno) {
    ++ _test_state.current_result->assert_count;

    if (proc->pid <= 0) {
        _test_fail(
            expr, filename, func_name, lineno,
            "illegal pid for %s %s: %d", BINARY_PATH, proc->filename, proc->pid
        );
    }

    struct timespec waited_at = { .tv_sec = 0, .tv_nsec = 0 };
    struct timespec ended_at  = { .tv_sec = 0, .tv_nsec = 0 };
    int status = 0;
    pid_t wait_res = -1;

    clock_gettime(CLOCK_MONOTONIC, &waited_at);
    while ((wait_res = waitpid(proc->pid, &status, WNOHANG)) == 0) {
        usleep(250000);
        clock_gettime(CLOCK_MONOTONIC, &ended_at);
        if (timespec_sub(&ended_at, &waited_at).tv_sec >= TEST_TIMEOUT) {
            _test_fail(
                expr, filename, func_name, lineno,
                "timeout running %s %s", BINARY_PATH, proc->filename
            );
        }
    }

    if (wait_res != proc->pid) {
        _test_fail(
            expr, filename, func_name, lineno,
            "error wating for %s %s: %s", BINARY_PATH, proc->filename, strerror(errno)
        );
    }

    if (WIFSIGNALED(status)) {
        _test_fail(
            expr, filename, func_name, lineno,
            "process %s %s terminated by signal %d", BINARY_PATH, proc->filename, WTERMSIG(status)
        );
    } else if (WIFSTOPPED(status)) {
        _test_fail(
            expr, filename, func_name, lineno,
            "process %s %s stopped by signal %d", BINARY_PATH, proc->filename, WSTOPSIG(status)
        );
    } else if (WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);
        if (exit_status != 0) {
            _test_fail(
                expr, filename, func_name, lineno,
                "process %s %s exit status: %d", BINARY_PATH, proc->filename, exit_status
            );
        }
    } else if (status != 0) {
        _test_fail(
            expr, filename, func_name, lineno,
            "process %s %s status: %d", BINARY_PATH, proc->filename, status
        );
    }
}

void _assert_proc_running(const ProcInfo *proc, const char *expr, const char *filename, const char *func_name, size_t lineno) {
    ++ _test_state.current_result->assert_count;

    int status = 0;
    if (waitpid(proc->pid, &status, WNOHANG) != 0) {
        _test_fail(
            expr, filename, func_name, lineno,
            "process isn't running %s %s", BINARY_PATH, proc->filename
        );
    }
}
