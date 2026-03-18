#define _GNU_SOURCE
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
}

void make_file(const char *suffix) {
    make_dir(NULL);

    char filename[4096];
    int res = snprintf(filename, sizeof(filename), "/tmp/test.wait_for_exist.%d/%s", getpid(), suffix);
    test_assertf(res > 0 && res < sizeof(filename), "create file name");
    int fd = open(filename, O_CREAT | O_WRONLY | O_CLOEXEC, 0640);
    int errnum = errno;
    close(fd);
    test_assertf(fd >= 0, "error creating file %s: %s", filename, strerror(errnum));
}

void rm(const char *suffix) {
    char filename[4096];
    int res = snprintf(filename, sizeof(filename), "/tmp/test.wait_for_exist.%d/%s", getpid(), suffix);
    test_assertf(res > 0 && res < sizeof(filename), "create file name");
    test_assertf(remove(filename) == 0, "error deleting %s: %s", filename, strerror(errno));
}

ProcInfo spawn_wait_for_exist(const char *suffix) {
    char buf[4096];
    pid_t pid = -1;

    int res = snprintf(buf, sizeof(buf), "/tmp/test.wait_for_exist.%d/%s", getpid(), suffix);
    test_assertf(res > 0 && res < sizeof(buf), "create file name");
    char *filename = strdup(buf);
    test_assert(filename != NULL);

    char *binary_path = canonicalize_file_name(BINARY_PATH);
    test_assertf(binary_path != NULL, "error canonicalizing path %s: %s", BINARY_PATH, strerror(errno));

    const char*const args[] = {
        BINARY_PATH,
        filename,
        NULL,
    };

    struct timespec started_at = { .tv_sec = 0, .tv_nsec = 0 };
    clock_gettime(CLOCK_MONOTONIC, &started_at);

    res = posix_spawn(&pid, binary_path, NULL, NULL, (char *const*)args, NULL);

    free(binary_path);
    binary_path = NULL;

    test_assertf(
        res == 0,
        "error spawning process: %s %s: %s", BINARY_PATH, filename, strerror(errno)
    );

    return (ProcInfo){
        .filename = filename,
        .started_at = started_at,
        .pid = pid,
    };
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
