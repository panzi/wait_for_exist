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
    static void _test_cat(test_func_, NAME)();               \
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

typedef struct Test {
    const char *func_name;
    const char *filename;
    size_t lineno;
    bool skip;
    void (*test_func)();
} Test;

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

    Test *tests;
    TestResult *results;

    const Test *current_test;
    TestResult *current_result;

    int original_stdout;
    int original_stderr;
} TestState;

int test_main(int argc, char *argv[], Test *tests);

extern TestState _test_state;

#ifdef __cplusplus
}
#endif

#endif
