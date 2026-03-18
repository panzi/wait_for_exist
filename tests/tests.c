#define _GNU_SOURCE

#include "tests.h"
#include "test_utils.h"

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
