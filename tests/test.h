#ifndef TEST_H
#define TEST_H
#pragma once

#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "strbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define _test_str2(X) #X
#define _test_str(X) _test_str2(X)

#define _test_cat2(A, B) A##B
#define _test_cat(A, B) _test_cat2(A, B)

#define _TEST(NAME, SKIP)                                    \
    void _test_cat(test_func_, NAME)();                      \
    Test _test_cat(test_case_, NAME) = {                     \
        .func_name = _test_str(NAME),                        \
        .filename = __FILE__,                                \
        .lineno = __LINE__,                                  \
        .skip = SKIP,                                        \
        .test_func = _test_cat(test_func_, NAME),            \
    };                                                       \
    void _test_cat(test_func_, NAME)()

#define TEST(NAME) _TEST(NAME, false)
#define TODO_TEST(NAME) _TEST(NAME, true)
#define DECL_TEST(NAME) extern Test _test_cat(test_case_, NAME);
#define GET_TEST(NAME) _test_cat(test_case_, NAME)

void _test_fail(
    const char *expr, const char *filename,
    const char *func_name, size_t lineno,
    const char *fmt, ...
) __attribute__ ((__noreturn__))
  __attribute__ ((format(printf, 5, 6)));

#define test_fail(FMT, ...)                                             \
    {                                                                   \
        ++ _test_state.current_result->assert_count;                    \
        _test_fail(                                                     \
            NULL, __FILE__, __FUNCTION__, __LINE__,                     \
            FMT __VA_OPT__(,) __VA_ARGS__                               \
        );                                                              \
    }

#define test_assertf(EXPR, FMT, ...)                                    \
    {                                                                   \
        ++ _test_state.current_result->assert_count;                    \
                                                                        \
        if (!(EXPR)) {                                                  \
            _test_fail(                                                 \
                NULL, __FILE__, __FUNCTION__, __LINE__,                 \
                FMT __VA_OPT__(,) __VA_ARGS__                           \
            );                                                          \
        }                                                               \
    }

#define test_assert(EXPR) test_assertf(EXPR, NULL)

typedef struct Test {
    const char *func_name;
    const char *filename;
    size_t lineno;
    bool skip;
    void (*test_func)();
} Test;

typedef struct TestCleanup {
    void (*cleanup_func)(void *data);
    void *data;
} TestCleanup;

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

enum {
    TEST_SIGINT  = 1,
    TEST_SIGTERM = 2,
    TEST_SIGALRM = 4,
};

typedef struct TestState {
    jmp_buf env;

    Test *tests;
    TestResult *results;

    const Test *current_test;
    TestResult *current_result;

    int original_stdout;
    int original_stderr;

    TestCleanup *cleanup;
    size_t cleanup_capacity;
    size_t cleanup_used;

    unsigned int signals;
} TestState;

/** Not thread safe! Add lock? */
void test_cleanup(void (*cleanup_func)(void *data), void *data);

int test_main(int argc, char *argv[], Test *tests);

extern TestState _test_state;

#ifdef __cplusplus
}
#endif

#endif
