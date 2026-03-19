#ifndef TEST_UTILS_H
#define TEST_UTILS_H
#pragma once

#include <time.h>

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

ProcInfo *_spawn_wait_for_exist(const char *suffix, const char *expr, const char *filename, const char *func_name, size_t lineno);

#define spawn_wait_for_exist(suffix) \
    _spawn_wait_for_exist((suffix), "spawn_wait_for_exist(" _test_str(suffix) ")", __FILE__, __FUNCTION__, __LINE__)

void proc_destroy(ProcInfo *proc);

void _assert_proc_ok(const ProcInfo *proc, const char *expr, const char *filename, const char *func_name, size_t lineno);

#define assert_proc_ok(PROC) \
    _assert_proc_ok((PROC), "assert_proc_ok(" _test_str(PROC) ")", __FILE__, __FUNCTION__, __LINE__)

void _assert_proc_running(const ProcInfo *proc, const char *expr, const char *filename, const char *func_name, size_t lineno);

#define assert_proc_running(PROC) \
    _assert_proc_running((PROC), "assert_proc_running(" _test_str(PROC) ")", __FILE__, __FUNCTION__, __LINE__)

#ifdef __cplusplus
}
#endif

#endif
