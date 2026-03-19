#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/epoll.h>

#include "test.h"

#define CLEAR "\x1B[0m"
#define BOLD "\x1B[1m"
#define RED "\x1B[31m"
#define GREEN "\x1B[32m"
#define YELLOW "\x1B[33m"
#define HIDE_CURSOR "\x1B[?15l"
#define SHOW_CURSOR "\x1B[?15h"

void _test_fail(const char *expr, const char *filename, const char *func_name, size_t lineno, const char *fmt, ...) {
    _test_state.current_result->ok = false;
    _test_state.current_result->assert_filename = filename;
    _test_state.current_result->assert_func_name = func_name;
    _test_state.current_result->assert_lineno = lineno;
    _test_state.current_result->assert_expr = expr;
    _test_state.current_result->assert_message = NULL;

    if (fmt != NULL && strlen(fmt) > 0) {
        va_list ap;
        va_start(ap, fmt);

        if (vasprintf(&_test_state.current_result->assert_message, fmt, ap) < 0) {
            _test_state.current_result->assert_message = strdup(strerror(errno));
        }

        va_end(ap);
    }
    longjmp(_test_state.env, -1);
}

void print_str(const char *str) {
    fwrite(str, strlen(str), 1, stdout);
}

void print_test_name(const char *func_name) {
    const char *ptr = func_name;
    while (*ptr) {
        const char *nextptr = strchr(ptr, '_');
        if (nextptr == NULL) {
            print_str(ptr);
            break;
        }

        fwrite(ptr, nextptr - ptr, 1, stdout);
        putchar(' ');
        ptr = nextptr + 1;
    }
}

void print_test_header(const Test *test, size_t width) {
    int count = 0;

    printf("%s:%" PRIuPTR ":%n", test->filename, test->lineno, &count);
    print_test_name(test->func_name);
    print_str(" ...");

    count += 5 + strlen(test->func_name);

    while (count < width) {
        putchar('.');
        count ++;
    }
    putchar(' ');
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
    printf("%s: ", test->filename);
    print_test_name(test->func_name);
    putchar('\n');
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
        print_str("    stdout\n");
        print_str("--------------------------------------------------------------------------------\n");
        putchar('\n');
        print_str(result->test_stdout.data);
        putchar('\n');
    }

    if (result->test_stderr.used > 0) {
        print_str("--------------------------------------------------------------------------------\n");
        print_str("    stderr\n");
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

TestState _test_state = {
    .tests = NULL,
    .results = NULL,
    .current_test = NULL,
    .current_result = NULL,
    .original_stdout = STDOUT_FILENO,
    .original_stderr = STDERR_FILENO,
    .cleanup = NULL,
    .cleanup_capacity = 0,
    .cleanup_used     = 0,
};

void test_cleanup(void (*cleanup_func)(void *data), void *data) {
    if (_test_state.cleanup_used >= _test_state.cleanup_capacity) {
        size_t new_capacity = _test_state.cleanup_capacity == 0 ? 32 : _test_state.cleanup_capacity * 2;
        if (new_capacity <= _test_state.cleanup_capacity || new_capacity >= SIZE_MAX / sizeof(TestCleanup)) {
            test_fail("failed to allocate test cleanup array");
        }
        TestCleanup *new_cleanup = realloc(_test_state.cleanup, new_capacity * sizeof(TestCleanup));
        if (new_cleanup == NULL) {
            test_fail("failed to allocate test cleanup array");
        }

        _test_state.cleanup = new_cleanup;
        _test_state.cleanup_capacity = new_capacity;
    }

    _test_state.cleanup[_test_state.cleanup_used ++] = (TestCleanup) {
        .cleanup_func = cleanup_func,
        .data = data,
    };
}

void handle_sigalarm(int signum) {
    _test_state.signals |= TEST_SIGALRM;
}

void handle_sigterm(int signum) {
    _test_state.signals |= TEST_SIGTERM;
}

void handle_sigint(int signum) {
    _test_state.signals |= TEST_SIGINT;
}

int test_main(int argc, char *argv[], Test *tests) {
    bool fail = false;
    bool use_color = isatty(STDOUT_FILENO) != 0;

    const char *filter_test = NULL;

    static const struct option long_options[] = {
        { "color",   required_argument,  0,  'c' },
        { "test",    required_argument,  0,  't' },
        { 0,         0,                  0,   0  },
    };

    for (;;) {
        int opt = getopt_long(argc, argv, "c:t:", long_options, NULL);

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

            case 't':
                filter_test = optarg;
                break;

            case '?':
                return 1;
        }
    }

    _test_state.tests = tests;

    size_t test_count = 0;
    for (Test *ptr = _test_state.tests; ptr->func_name; ++ ptr) {
        ++ test_count;
        if (filter_test && strcasestr(ptr->func_name, filter_test) == NULL) {
            ptr->skip = true;
        }
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

    signal(SIGALRM, handle_sigalarm);
    signal(SIGINT,  handle_sigint);
    signal(SIGTERM, handle_sigterm);

    size_t max_width = 0;

    for (const Test *test = _test_state.tests; test->func_name; ++ test) {
        size_t width = strlen(test->filename) + strlen(test->func_name) + 7;
        size_t lineno = test->lineno;
        if (lineno == 0) {
            ++ width;
        } else {
            while (lineno) {
                ++ width;
                lineno /= 10;
            }
        }

        if (width > max_width) {
            max_width = width;
        }
    }

    for (size_t index = 0; index < test_count; ++ index) {
        if (_test_state.signals & TEST_SIGINT) {
            puts("Stopping on SIGINT!");
            break;
        }

        if (_test_state.signals & TEST_SIGTERM) {
            puts("Stopping on SIGTERM!");
            break;
        }

        const Test *test = &_test_state.tests[index];
        TestResult *result = &_test_state.results[index];

        _test_state.current_test = test;
        _test_state.current_result = result;

        print_test_header(test, max_width);

        if (test->skip) {
            print_colored(use_color, BOLD YELLOW, "SKIP");
            putchar('\n');
        } else {
            HANDLE_ERROR(pipe2(capture_stdout_pipe, O_CLOEXEC));
            HANDLE_ERROR(pipe2(capture_stderr_pipe, O_CLOEXEC));

            HANDLE_ERROR(dup2(capture_stdout_pipe[1], STDOUT_FILENO));
            HANDLE_ERROR(dup2(capture_stderr_pipe[1], STDERR_FILENO));

            close(capture_stdout_pipe[1]);
            close(capture_stderr_pipe[1]);

            capture_stdout_pipe[1] = -1;
            capture_stderr_pipe[1] = -1;

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

            for (size_t index = 0; index < _test_state.cleanup_used; ++ index) {
                const TestCleanup *cleanup = &_test_state.cleanup[index];
                cleanup->cleanup_func(cleanup->data);
            }

            _test_state.cleanup_used = 0;

            close(STDOUT_FILENO);
            close(STDERR_FILENO);

            int join_res = pthread_join(thread, NULL);
            errnum = errno;

            close(capture_stdout_pipe[0]);
            close(capture_stderr_pipe[0]);

            capture_stdout_pipe[0] = -1;
            capture_stderr_pipe[0] = -1;

            dup2(original_stderr, STDERR_FILENO);
            dup2(original_stdout, STDOUT_FILENO);

            if (join_res != 0) {
                fprintf(
                    stderr,
                    "%s:%s: pthread_join(thread, NULL): %s\n",
                    test->filename,
                    test->func_name,
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
    _test_state.results = NULL;

    free(_test_state.cleanup);

    _test_state.cleanup = NULL;
    _test_state.cleanup_capacity = 0;
    _test_state.cleanup_used = 0;

    return fail ? 1 : 0;
}
