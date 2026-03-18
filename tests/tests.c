#define _GNU_SOURCE

#include "tests.h"
#include "test_utils.h"

#include "normpath.h"

// TODO: free() on fail
#define assert_normpath(path, expected) \
{ \
    const char *_test_path = (path); \
    const char *_test_expected = (expected); \
    char *_test_path_res = normpath(_test_path); \
    test_assertf(_test_path_res != NULL, "normpath(\"%s\") failed: %s", _test_path, strerror(errno)); \
    test_assertf(strcmp(_test_path_res, _test_expected) == 0, "expected: \"%s\", actual: \"%s\"", _test_expected, _test_path_res); \
    free(_test_path_res); \
}

TODO_TEST(normpath_simple) {}
TODO_TEST(normpath_double_slash) {}
TODO_TEST(normpath_trailing_slash) {}
TODO_TEST(normpath_dot) {}
TODO_TEST(normpath_double_dot) {}
TODO_TEST(normpath_everything) {}

TEST(path_existing) {
    make_dir(NULL);
    make_dir("path_existing");
    make_file("path_existing/exsiting.txt");
    ProcInfo proc = spawn_wait_for_exist("path_existing/exsiting.txt");
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
    make_dir(NULL);
    make_dir("long_path");
    // TODO
}

TODO_TEST(complex) {
    // TODO
}
