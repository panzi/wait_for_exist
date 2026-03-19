#define _GNU_SOURCE

#include "tests.h"
#include "test_utils.h"

#include "normpath.h"

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

void _assert_normpath(
        const char *path,
        const char *expected,
        const char *expr,
        const char *filename,
        const char *func_name,
        size_t lineno
) {
    ++ _test_state.current_result->assert_count;

    char *actual = normpath(path);
    if (actual == NULL) {
        _test_fail(
            expr, filename, func_name, lineno,
            "normpath(\"%s\")", path
        );
    }
    test_cleanup(free, actual);
    if (strcmp(actual, expected) != 0) {
        _test_fail(
            expr, filename, func_name, lineno,
            "    expected: \"%s\"\n      actual: \"%s\"",
            expected, actual
        );
    }
}

#define assert_normpath(path, expected) \
    _assert_normpath(                   \
        (path),                         \
        (expected),                     \
        "assert_normpath(" _test_str(path) ", " _test_str(expected) ")", \
        __FILE__,                       \
        __FUNCTION__,                   \
        __LINE__                        \
    )

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
    ProcInfo *proc = spawn_wait_for_exist("parent_exists/wanted.txt");
    usleep(1000);
    assert_proc_running(proc);
    make_file("parent_exists/wanted.txt");
    assert_proc_ok(proc);
}

TEST(deep_path) {
    make_dir(NULL);
    make_dir("deep_path");
    ProcInfo *proc = spawn_wait_for_exist("deep_path/foo/bar/baz/wanted.txt");

    usleep(1000);
    assert_proc_running(proc);

    make_dir("deep_path/foo");
    make_dir("deep_path/foo/bar");
    usleep(2000);

    assert_proc_running(proc);

    make_dir("deep_path/foo/bar/baz");
    make_file("deep_path/foo/bar/baz/wanted.txt");

    assert_proc_ok(proc);
}

TEST(create_and_delete) {
    make_dir(NULL);
    make_dir("create_and_delete");
    ProcInfo *proc = spawn_wait_for_exist("create_and_delete/foo/bar/baz/wanted.txt");
    assert_proc_running(proc);

    make_dir("create_and_delete/foo");
    make_dir("create_and_delete/foo/unrelated");
    make_dir("create_and_delete/foo/bar");

    usleep(1000);
    assert_proc_running(proc);

    rm("create_and_delete/foo/unrelated");
    rm("create_and_delete/foo/bar");
    rm("create_and_delete/foo");
    rm("create_and_delete");

    assert_proc_running(proc);

    make_dir("create_and_delete");
    make_dir("create_and_delete/unrelated");
    make_dir("create_and_delete/foo");
    make_dir("create_and_delete/foo/unrelated");
    make_dir("create_and_delete/foo/bar");
    make_file("create_and_delete/foo/bar/unrelated.txt");

    assert_proc_running(proc);

    rm("create_and_delete/foo/bar/unrelated.txt");
    make_dir("create_and_delete/foo/bar/baz");
    rm("create_and_delete/foo/bar/baz");

    assert_proc_running(proc);

    make_dir("create_and_delete/foo/bar/baz");
    make_file("create_and_delete/foo/bar/baz/unrelated.txt");
    rm("create_and_delete/foo/bar/baz/unrelated.txt");

    assert_proc_running(proc);

    make_file("create_and_delete/foo/bar/baz/wanted.txt");

    assert_proc_ok(proc);
}
