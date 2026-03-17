#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <inttypes.h>
#include <spawn.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/epoll.h>

typedef struct Test {
    const char *func_name;
    const char *filename;
    size_t lineno;
    bool skip;
    void (*test_func)();
} Test;


#define _test_str2(X) #X
#define _test_str(X) _test_str2(X)

#define _test_cat2(A, B) A##B
#define _test_cat(A, B) _test_cat2(A, B)

#define _TEST(NAME, SKIP)                                    \
    static void _test_cat(test_func_, NAME)();               \
    const Test _test_cat(test_case_, NAME) = {               \
        .func_name = _test_str(NAME),                        \
        .filename = __FILE__,                                \
        .lineno = __LINE__,                                  \
        .skip = SKIP,                                        \
        .test_func = _test_cat(test_func_, NAME),            \
    };                                                       \
    void _test_cat(test_func_, NAME)()

#define TEST(NAME) _TEST(NAME, false)
#define TODO_TEST(NAME) _TEST(NAME, true)

typedef struct StrBuf {
    char *data;
    size_t capacity;
    size_t used;
} StrBuf;

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

typedef struct TestResult {
    bool ok;
    size_t assert_count;
    const char *assert_filename;
    const char *assert_func_name;
    size_t assert_lineno;
    const char *assert_expr;
    char *assert_message;
    StrBuf test_stdout;
    StrBuf test_stderr;
} TestResult;

typedef struct TestState {
    jmp_buf env;

    const Test *tests;
    TestResult *results;

    const Test *current_test;
    TestResult *current_result;

    int original_stdout;
    int original_stderr;
} TestState;

TestState _test_state;

#define test_assertf(EXPR, FMT, ...)                                        \
    {                                                                       \
        ++ _test_state.current_result->assert_count;                        \
                                                                            \
        if (!(EXPR)) {                                                      \
            _test_state.current_result->ok = false;                         \
            _test_state.current_result->assert_filename = __FILE__;         \
            _test_state.current_result->assert_func_name = __FUNCTION__;    \
            _test_state.current_result->assert_lineno = __LINE__;           \
            _test_state.current_result->assert_expr = _test_str(EXPR);      \
            _test_state.current_result->assert_message = NULL;              \
                                                                            \
            const char *_test_fmt = FMT;                                    \
            if (_test_fmt != NULL && strlen(_test_fmt) > 0) {               \
                if (asprintf(&_test_state.current_result->assert_message,   \
                    FMT __VA_OPT__(,) __VA_ARGS__) < 0) {                   \
                    _test_state.current_result->assert_message = strdup(strerror(errno)); \
                }                                                           \
            }                                                               \
            longjmp(_test_state.env, -1);                                   \
        }                                                                   \
    }

#define test_assert(EXPR) test_assertf(EXPR, NULL)

#define CLEAR "\x1B[0m"
#define BOLD "\x1B[1m"
#define RED "\x1B[31m"
#define GREEN "\x1B[32m"
#define YELLOW "\x1B[33m"
#define HIDE_CURSOR "\x1B[?15l"
#define SHOW_CURSOR "\x1B[?15h"

void print_str(const char *str) {
    fwrite(str, strlen(str), 1, stdout);
}

void print_test_header(const Test *test) {
    printf("%s:%" PRIuPTR ":%s ... ", test->filename, test->lineno, test->func_name);
    fflush(stdout);
}

#define print_colored(use_color, color, fmt, ...) \
    if (use_color) { \
        print_str(color); \
    } \
    printf(fmt __VA_OPT__(,) __VA_ARGS__); \
    if (use_color) { \
        print_str(CLEAR); \
    }

void print_pass_fail(const TestResult *result, bool use_color) {
    if (result->ok) {
        print_colored(use_color, BOLD GREEN, "PASS");
    } else {
        print_colored(use_color, BOLD RED, "FAIL");
    }
    putchar('\n');
}

void print_fail_details(const Test *test, const TestResult *result) {
    print_str("================================================================================\n");
    printf("%s: %s\n", test->filename, test->func_name);
    print_str("================================================================================\n");
    putchar('\n');
    printf(
        "%s:%" PRIuPTR ":%s: %s\n",
        result->assert_filename,
        result->assert_lineno,
        result->assert_func_name,
        result->assert_expr == NULL ? "" : result->assert_expr
    );
    putchar('\n');
    if (result->assert_message != NULL) {
        printf("%s\n", result->assert_message);
        putchar('\n');
    } else if (result->assert_count == 0) {
        print_str("No asserts!\n");
        putchar('\n');
    }

    if (result->test_stdout.used > 0) {
        print_str("--------------------------------------------------------------------------------\n");
        print_str("stdout\n");
        print_str("--------------------------------------------------------------------------------\n");
        putchar('\n');
        print_str(result->test_stdout.data);
        putchar('\n');
    }

    if (result->test_stderr.used > 0) {
        print_str("--------------------------------------------------------------------------------\n");
        print_str("stderr\n");
        print_str("--------------------------------------------------------------------------------\n");
        putchar('\n');
        print_str(result->test_stderr.data);
        putchar('\n');
    }
}

void print_summary(bool use_color) {
    size_t test_count = 0;
    size_t skip_count = 0;
    size_t pass_count = 0;
    size_t fail_count = 0;

    for (const Test *test = _test_state.tests; test->func_name; ++ test) {
        const TestResult *result = &_test_state.results[test_count];
        ++ test_count;
        if (test->skip) {
            ++ skip_count;
        } else if (result->ok) {
            ++ pass_count;
        } else {
            ++ fail_count;

            print_fail_details(test, result);
        }
    }

    printf("%" PRIuPTR " tests in total, ", test_count);

    print_colored(use_color && pass_count > 0, GREEN, "%" PRIuPTR, pass_count);
    print_str(" passed, ");

    if (skip_count > 0) {
        print_colored(use_color, YELLOW, "%" PRIuPTR, skip_count);
        print_str(" skipped, ");
    }

    print_colored(use_color && fail_count > 0, RED, "%" PRIuPTR, fail_count);
    print_str(" failed.\n");
}

typedef struct CaptureThreadData {
    int stdout_fd;
    int stderr_fd;
    StrBuf *stdout_buf;
    StrBuf *stderr_buf;
} CaptureThreadData;

#define MAX_EVENTS 64

void *capture_thread_func(void *ptr) {
    CaptureThreadData *data = (CaptureThreadData*)ptr;

    //FILE *orig_stderr = fdopen(dup(_test_state.original_stderr), "w");
    //fprintf(orig_stderr, "capture_thread_func start\n");

    struct epoll_event events[MAX_EVENTS];

    int epoll = epoll_create1(EPOLL_CLOEXEC);

    if (epoll < 0) {
        //fprintf(orig_stderr, "epoll_create1(EPOLL_CLOEXEC): %s\n", strerror(errno));
        //fclose(orig_stderr);
        pthread_exit(NULL);
        return NULL;
    }

    epoll_ctl(epoll, EPOLL_CTL_ADD, data->stdout_fd, &(struct epoll_event){ .events = EPOLLIN | EPOLLRDHUP, .data.fd = data->stdout_fd });
    epoll_ctl(epoll, EPOLL_CTL_ADD, data->stderr_fd, &(struct epoll_event){ .events = EPOLLIN | EPOLLRDHUP, .data.fd = data->stderr_fd });

    bool stdout_open = true;
    bool stderr_open = true;

    //fprintf(orig_stderr, "capture_thread_func epoll = %d\n", epoll);
    //fprintf(orig_stderr, "capture_thread_func data->stdout_fd = %d\n", data->stdout_fd);
    //fprintf(orig_stderr, "capture_thread_func data->stderr_fd = %d\n", data->stderr_fd);

    char buf[BUFSIZ];

    while (stdout_open || stderr_open) {
        //fprintf(orig_stderr, "wait...\n");
        int event_count = epoll_wait(epoll, events, MAX_EVENTS, -1);
        if (event_count <= 0) {
            break;
        }

        for (int index = 0; index < event_count; ++ index) {
            const struct epoll_event *event = &events[index];
            //const char *what = event->data.fd == data->stdout_fd ? "stdout" : "stderr";
            if (event->events & EPOLLIN) {
                //fprintf(orig_stderr, "capture_thread_func %s POLLIN\n", what);
                ssize_t byte_count = read(event->data.fd, buf, BUFSIZ);
                //fprintf(orig_stderr, "capture_thread_func %s read %ld bytes\n", what, byte_count);
                if (byte_count > 0) {
                    StrBuf *strbuf = event->data.fd == data->stdout_fd ? data->stdout_buf : data->stderr_buf;
                    strbuf_append(strbuf, buf, byte_count);
                }
            }

            if (event->events & EPOLLRDHUP) {
                //fprintf(orig_stderr, "capture_thread_func %s EPOLLRDHUP\n", what);
                if (event->data.fd == data->stdout_fd) {
                    stdout_open = false;
                } else {
                    stderr_open = false;
                }
            }

            if (event->events & EPOLLHUP) {
                //fprintf(orig_stderr, "capture_thread_func %s %d EPOLLHUP\n", what, event->data.fd);
                if (event->data.fd == data->stdout_fd) {
                    stdout_open = false;
                } else {
                    stderr_open = false;
                }
            }
        }
    }

    //fprintf(orig_stderr, "capture_thread_func exit\n");

    close(epoll);
    //fclose(orig_stderr);

    pthread_exit(NULL);
    return NULL;
}

int main(int argc, char *argv[]) {
    bool fail = false;
    bool use_color = isatty(STDOUT_FILENO) != 0;

    static const struct option long_options[] = {
        { "color",   required_argument,  0,  'c' },
        { 0,         0,                  0,   0  },
    };

    for (;;) {
        int opt = getopt_long(argc, argv, "c:", long_options, NULL);

        if (opt == -1)
            break;

        switch (opt) {
            case 'c':
                if (strcasecmp(optarg, "auto") == 0) {
                    use_color = isatty(STDOUT_FILENO) != 0;
                } else if (strcasecmp(optarg, "always") == 0) {
                    use_color = true;
                } else if (strcasecmp(optarg, "never") == 0) {
                    use_color = false;
                } else {
                    fprintf(stderr, "illegal argument for option --color=%s\n", optarg);
                    return 1;
                }
                break;

            case '?':
                return 1;
        }
    }

    size_t test_count = 0;
    for (const Test *ptr = _test_state.tests; ptr->func_name; ++ ptr) {
        ++ test_count;
    }

    _test_state.results = calloc(test_count, sizeof(TestResult));
    if (_test_state.results == NULL) {
        perror("allocate results array");
        return 1;
    }

    for (size_t index = 0; index < test_count; ++ index) {
        _test_state.results[index].ok = true;
    }

    if (use_color) {
        print_str(HIDE_CURSOR);
        fflush(stdout);
    }

    int capture_stdout_pipe[2] = { -1, -1 };
    int capture_stderr_pipe[2] = { -1, -1 };
    int original_stdout = -1;
    int original_stderr = -1;

#define HANDLE_ERROR(EXPR) if ((EXPR) < 0) { perror(_test_str(EXPR)); fail = true; goto cleanup; }

    HANDLE_ERROR(original_stdout = dup(STDOUT_FILENO));
    HANDLE_ERROR(original_stderr = dup(STDERR_FILENO));

    HANDLE_ERROR(fcntl(original_stdout, F_SETFD, FD_CLOEXEC));
    HANDLE_ERROR(fcntl(original_stderr, F_SETFD, FD_CLOEXEC));

    _test_state.original_stdout = original_stdout;
    _test_state.original_stderr = original_stderr;

    signal(SIGALRM, SIG_IGN);

    for (size_t index = 0; index < test_count; ++ index) {
        const Test *test = &_test_state.tests[index];
        TestResult *result = &_test_state.results[index];

        _test_state.current_test = test;
        _test_state.current_result = result;

        print_test_header(test);

        if (test->skip) {
            print_colored(use_color, BOLD YELLOW, "SKIP");
            putchar('\n');
        } else {
            HANDLE_ERROR(pipe2(capture_stdout_pipe, O_CLOEXEC));
            HANDLE_ERROR(pipe2(capture_stderr_pipe, O_CLOEXEC));

            HANDLE_ERROR(dup2(capture_stdout_pipe[1], STDOUT_FILENO));
            HANDLE_ERROR(dup2(capture_stderr_pipe[1], STDERR_FILENO));

            CaptureThreadData capture_thread_data = {
                .stdout_fd = capture_stdout_pipe[0],
                .stderr_fd = capture_stderr_pipe[0],
                .stdout_buf = &result->test_stdout,
                .stderr_buf = &result->test_stderr,
            };

            pthread_t thread;
            int errnum = pthread_create(&thread, NULL, capture_thread_func, &capture_thread_data);
            if (errnum != 0) {
                fprintf(
                    stderr,
                    "pthread_create(&thread, NULL, capture_thread_func, &capture_thread_data): %s\n",
                    strerror(errnum)
                );
                fail = true;
                goto cleanup;
            }

            if (setjmp(_test_state.env) == 0) {
                test->test_func();
            }

            close(capture_stdout_pipe[1]);
            close(capture_stderr_pipe[1]);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);

            capture_stdout_pipe[1] = -1;
            capture_stderr_pipe[1] = -1;

            int join_res = pthread_join(thread, NULL);
            errnum = errno;

            close(capture_stdout_pipe[0]);
            close(capture_stderr_pipe[0]);

            capture_stdout_pipe[0] = -1;
            capture_stderr_pipe[0] = -1;

            HANDLE_ERROR(dup2(original_stderr, STDERR_FILENO));
            HANDLE_ERROR(dup2(original_stdout, STDOUT_FILENO));

            if (join_res != 0) {
                fprintf(
                    stderr,
                    "pthread_join(thread, NULL): %s\n",
                    strerror(errnum)
                );
                fail = true;
                goto cleanup;
            }

            if (result->ok && result->assert_count == 0) {
                result->ok = false;
                result->assert_filename = test->filename;
                result->assert_func_name = test->func_name;
                result->assert_lineno = test->lineno;
            }

            print_pass_fail(result, use_color);
        }

        if (!result->ok) {
            fail = true;
        }
    }

    putchar('\n');
    print_summary(use_color);

    if (use_color) {
        print_str(SHOW_CURSOR);
        fflush(stdout);
    }

cleanup:
    if (original_stdout >= 0) {
        close(original_stdout);
        original_stdout = -1;
    }

    if (original_stderr >= 0) {
        close(original_stderr);
        original_stderr = -1;
    }

    if (capture_stdout_pipe[0] != -1) {
        close(capture_stdout_pipe[0]);
    }

    if (capture_stdout_pipe[1] != -1) {
        close(capture_stdout_pipe[1]);
    }

    if (capture_stderr_pipe[0] != -1) {
        close(capture_stderr_pipe[0]);
    }

    if (capture_stderr_pipe[1] != -1) {
        close(capture_stderr_pipe[1]);
    }

    for (size_t index = 0; index < test_count; ++ index) {
        TestResult *result = &_test_state.results[index];
        strbuf_destroy(&result->test_stdout);
        strbuf_destroy(&result->test_stderr);
        free(result->assert_message);
    }

    free(_test_state.results);

    return fail ? 1 : 0;
}

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
    test_assertf(fd >= 0, "error creating file %s: %s", filename, strerror(errno));
    close(fd);
}

void rm(const char *suffix) {
    char filename[4096];
    int res = snprintf(filename, sizeof(filename), "/tmp/test.wait_for_exist.%d/%s", getpid(), suffix);
    test_assertf(res > 0 && res < sizeof(filename), "create file name");
    test_assertf(remove(filename) == 0, "error deleting %s: %s", filename, strerror(errno));
}

typedef struct ProcInfo {
    char *filename;
    struct timespec started_at;
    pid_t pid;
} ProcInfo;

#ifndef BINARY_PATH
#define BINARY_PATH "./build/debug/wait_for_exist"
#endif

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

#define TIMEOUT 5

#define assert_proc_ok(PROC) { \
    alarm(TIMEOUT); /* HACK: using alarm() for timeout */ \
    int _test_status = 0; \
    int _test_wait_res = waitpid((PROC)->pid, &_test_status, 0); \
    \
    struct timespec _test_ended_at = { .tv_sec = 0, .tv_nsec = 0 }; \
    clock_gettime(CLOCK_MONOTONIC, &_test_ended_at); \
    \
    if (_test_wait_res == -1) { \
        int errnum = errno; \
        if (errnum == EINTR && timespec_sub(&_test_ended_at, &(PROC)->started_at).tv_sec >= TIMEOUT) { \
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
    free((PROC)->filename); \
    (PROC)->filename = NULL; \
    sleep(0); /* HACK: clear alarm */ \
}

#define assert_proc_running(PROC) { \
    int _test_proc_status = 0; \
    test_assertf(waitpid((PROC)->pid, &_test_proc_status, WNOHANG) == 0, "process isn't running %s %s", BINARY_PATH, (PROC)->filename); \
}

TEST(existing) {
    make_dir(NULL);
    make_dir("existing");
    make_file("existing/exsiting.txt");
    ProcInfo proc = spawn_wait_for_exist("existing/exsiting.txt");
    assert_proc_ok(&proc);
}

TEST(parent_exists) {
    make_dir(NULL);
    make_dir("parent_exists");
    ProcInfo proc = spawn_wait_for_exist("parent_exists/new.txt");
    usleep(1000);
    assert_proc_running(&proc);
    make_file("parent_exists/new.txt");
    assert_proc_ok(&proc);
}

TODO_TEST(long_path) {
    // TODO
}

TODO_TEST(complex) {
    // TODO
}

TestState _test_state = {
    .tests = (Test[]){
        test_case_existing,
        test_case_parent_exists,
        test_case_long_path,
        test_case_complex,
        { NULL, NULL },
    },
    .results = NULL,
    .current_test = NULL,
    .current_result = NULL,
    .original_stdout = STDOUT_FILENO,
    .original_stderr = STDERR_FILENO,
};
