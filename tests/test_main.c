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

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

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

typedef struct TestResult {
    bool ok;
    size_t assert_count;
    const char *assert_filename;
    const char *assert_func_name;
    size_t assert_lineno;
    const char *assert_expr;
    char *assert_message;
} TestResult;

typedef struct TestState {
    jmp_buf env;

    const Test *tests;
    TestResult *results;

    const Test *current_test;
    TestResult *current_result;
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
                    FMT __VA_OPT__(,) __VA_ARGS__) < 0) {             \
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

int main(int argc, char *argv[]) {
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

    bool fail = false;

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
            if (setjmp(_test_state.env) == 0) {
                test->test_func();
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

    for (size_t index = 0; index < test_count; ++ index) {
        TestResult *result = &_test_state.results[index];
        free(result->assert_message);
    }

    free(_test_state.results);

    if (use_color) {
        print_str(SHOW_CURSOR);
        fflush(stdout);
    }

    return fail ? 1 : 0;
}

void make_dir(const char *suffix) {
    char filename[4096];
    int res = suffix == NULL ?
        snprintf(filename, sizeof(filename), "/tmp/test.wait_for_exist.%d", getpid()) :
        snprintf(filename, sizeof(filename), "/tmp/test.wait_for_exist.%d/%s", getpid(), suffix);
    test_assertf(res > 0 && res < sizeof(filename), "create directory name");
    test_assertf(mkdir(filename, 0750) == 0 && errno != EEXIST, "error creating directory %s: %s", filename, strerror(errno));
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
    const char *filename;
    struct timespec started_at;
    pid_t pid;
} ProcInfo;

#ifndef BINARY_PATH
#define BINARY_PATH "./build/debug/wait_for_exist"
#endif

ProcInfo spawn_wait_for_exist(const char *suffix) {
    pid_t pid = -1;

    char output[4096];
    int res = snprintf(output, sizeof(output), "/tmp/test.wait_for_exist.%d/output.txt", getpid());
    test_assertf(res > 0 && res < sizeof(output), "create file name");

    char filename[4096];
    res = snprintf(filename, sizeof(filename), "/tmp/test.wait_for_exist.%d/%s", getpid(), suffix);
    test_assertf(res > 0 && res < sizeof(filename), "create file name");

    posix_spawn_file_actions_t file_actions;
    test_assert(posix_spawn_file_actions_init(&file_actions) == 0);
    test_assert(posix_spawn_file_actions_addclose(&file_actions, STDOUT_FILENO) == 0);
    test_assert(posix_spawn_file_actions_addopen(&file_actions, STDOUT_FILENO, output, O_CREAT | O_WRONLY, 0640) == 0);
    test_assert(posix_spawn_file_actions_adddup2(&file_actions, STDOUT_FILENO, STDERR_FILENO) == 0);

    char *binary_path = canonicalize_file_name(BINARY_PATH);
    test_assertf(binary_path != NULL, "error canonicalizing path %s: %s", BINARY_PATH, strerror(errno));

    const char*const args[] = {
        BINARY_PATH,
        filename,
        NULL,
    };

    struct timespec started_at = { .tv_sec = 0, .tv_nsec = 0 };
    clock_gettime(CLOCK_MONOTONIC, &started_at);

    res = posix_spawn(&pid, binary_path, &file_actions, NULL, (char *const*)args, NULL);

    posix_spawn_file_actions_destroy(&file_actions);

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

#define TIMEOUT 5

#define assert_proc_ok(PROC) { \
    alarm(TIMEOUT); /* HACK: using alarm() for timeout */ \
    int status = 0; \
    int res = waitpid((PROC)->pid, &status, 0); \
    \
    struct timespec ended_at = { .tv_sec = 0, .tv_nsec = 0 }; \
    clock_gettime(CLOCK_MONOTONIC, &ended_at); \
    \
    if (res == -1) { \
        int errnum = errno; \
        if (errnum == EINTR && (ended_at.tv_sec - (PROC)->started_at.tv_sec) >= TIMEOUT) { \
            test_assertf(false, "timeout running %s %s", BINARY_PATH, (PROC)->filename); \
        } else { \
            test_assertf(res >= 0, "error wating for %s %s: %s", BINARY_PATH, (PROC)->filename, strerror(errnum)); \
        } \
    } else { \
        test_assertf(status == 0, "wait_for_exist exist status: %d", status); \
    } \
    sleep(0); /* HACK: clear alarm */ \
}

TEST(existing) {
    make_file("exsiting.txt");
    ProcInfo proc = spawn_wait_for_exist("exsiting.txt");
    assert_proc_ok(&proc);
}

TODO_TEST(parent_exists) {
    // TODO
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
};
