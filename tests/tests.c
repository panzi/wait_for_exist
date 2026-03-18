#define _GNU_SOURCE

#include "tests.h"
#include "test_utils.h"

#include "normpath.h"

#include <unistd.h>
#include <string.h>
#include <errno.h>

#define assert_normpath(path, expected) \
{ \
    const char *_test_path = (path); \
    const char *_test_expected = (expected); \
    char *_test_path_res = normpath(_test_path); \
    test_assertf(_test_path_res != NULL, "normpath(\"%s\") failed: %s", _test_path, strerror(errno)); \
    test_cleanup(free, _test_path_res); \
    test_assertf(strcmp(_test_path_res, _test_expected) == 0, "    expected: \"%s\"\n      actual: \"%s\"", _test_expected, _test_path_res); \
}

#define chdir_tmp() test_assertf(chdir("/tmp") == 0, "chdir(\"/tmp\"): %s", strerror(errno))

TEST(normpath_empty) {
    chdir_tmp();
    assert_normpath("", "/tmp");
}

TEST(normpath_simple) {
    chdir_tmp();
    assert_normpath("foo", "/tmp/foo");
}

TEST(normpath_absolute) {
    chdir_tmp();
    assert_normpath("/", "/");
    assert_normpath("/foo/bar", "/foo/bar");
}

TEST(normpath_double_slash) {
    chdir_tmp();
    assert_normpath("foo//bar", "/tmp/foo/bar");
}

TEST(normpath_trailing_slash) {
    chdir_tmp();
    assert_normpath("foo/", "/tmp/foo");
    assert_normpath("foo///", "/tmp/foo");
}

TEST(normpath_dot) {
    chdir_tmp();
    assert_normpath(".", "/tmp");
    assert_normpath("./foo", "/tmp/foo");
    assert_normpath("foo/.", "/tmp/foo");
    assert_normpath("foo/./bar", "/tmp/foo/bar");
}

TEST(normpath_double_dot) {
    chdir_tmp();
    assert_normpath("..", "/");
    assert_normpath("../foo", "/foo");
    assert_normpath("foo/..", "/tmp");
    assert_normpath("foo/bar/../", "/tmp/foo");
    assert_normpath("foo/../bar/../../..", "/");
}

TEST(normpath_everything) {
    chdir_tmp();
    assert_normpath("././foo/../bar//baz///", "/tmp/bar/baz");
    assert_normpath("/./../foo/../bar//baz///", "/bar/baz");
}

TEST(path_existing) {
    make_dir(NULL);
    make_dir("path_existing");
    make_file("path_existing/exsiting.txt");
    ProcInfo *proc = spawn_wait_for_exist("path_existing/exsiting.txt");
    assert_proc_ok(proc);
}

TEST(parent_exists) {
    make_dir(NULL);
    make_dir("parent_exists");
    ProcInfo *proc = spawn_wait_for_exist("parent_exists/new.txt");
    usleep(1000);
    assert_proc_running(proc);
    make_file("parent_exists/new.txt");
    assert_proc_ok(proc);
}

TEST(deep_path) {
    make_dir(NULL);
    make_dir("deep_path");
    ProcInfo *proc = spawn_wait_for_exist("deep_path/foo/bar/baz/new.txt");

    usleep(1000);
    assert_proc_running(proc);

    make_dir("deep_path/foo");
    make_dir("deep_path/foo/bar");
    usleep(2000);

    assert_proc_running(proc);

    make_dir("deep_path/foo/bar/baz");
    make_file("deep_path/foo/bar/baz/new.txt");

    assert_proc_ok(proc);
}

TODO_TEST(create_and_delete) {
    // TODO
}
