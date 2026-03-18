#define _GNU_SOURCE
#include "normpath.h"
#include "test.h"
#include "test_utils.h"

#include <stddef.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

void make_dir(const char *suffix) {
    char filename[4096];
    int res = suffix == NULL ?
        snprintf(filename, sizeof(filename), "/tmp/test.wait_for_exist.%d", getpid()) :
        snprintf(filename, sizeof(filename), "/tmp/test.wait_for_exist.%d/%s", getpid(), suffix);
    test_assertf(res > 0 && res < sizeof(filename), "create directory name");
    test_assertf(mkdir(filename, 0750) == 0 || errno == EEXIST, "error creating directory %s: %s", filename, strerror(errno));
    fprintf(stderr, "%s: NEW DIR\n", filename);
}

void make_file(const char *suffix) {
    char filename[4096];
    int res = snprintf(filename, sizeof(filename), "/tmp/test.wait_for_exist.%d/%s", getpid(), suffix);
    test_assertf(res > 0 && res < sizeof(filename), "create file name");
    int fd = open(filename, O_CREAT | O_WRONLY | O_CLOEXEC, 0640);
    int errnum = errno;
    close(fd);
    test_assertf(fd >= 0, "error creating file %s: %s", filename, strerror(errnum));
    fprintf(stderr, "%s: NEW FILE\n", filename);
}

void rm(const char *suffix) {
    char filename[4096];
    int res = snprintf(filename, sizeof(filename), "/tmp/test.wait_for_exist.%d/%s", getpid(), suffix);
    test_assertf(res > 0 && res < sizeof(filename), "create file name");
    test_assertf(remove(filename) == 0, "error deleting %s: %s", filename, strerror(errno));
    fprintf(stderr, "%s: DELETED\n", filename);
}

void _proc_destroy(void *ptr) {
    proc_destroy((ProcInfo*)ptr);
}

ProcInfo *spawn_wait_for_exist(const char *suffix) {
    char buf[4096];

    int res = snprintf(buf, sizeof(buf), "/tmp/test.wait_for_exist.%d/%s", getpid(), suffix);
    test_assertf(res > 0 && res < sizeof(buf), "create file name");
    char *filename = strdup(buf);
    test_assert(filename != NULL);

    ProcInfo *proc = malloc(sizeof(ProcInfo));
    if (proc == NULL) {
        free(filename);
        test_assert(proc != NULL);
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

    test_assertf(
        res == 0,
        "error spawning process: %s %s: %s", BINARY_PATH, filename, strerror(errno)
    );

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
        res.tv_sec = lhs->tv_sec - rhs->tv_sec - 1;
        res.tv_nsec = lhs->tv_nsec - rhs->tv_nsec + 1000000000;
    } else {
        res.tv_sec = lhs->tv_sec - rhs->tv_sec;
        res.tv_nsec = lhs->tv_nsec - rhs->tv_nsec;
    }
    return res;
}
