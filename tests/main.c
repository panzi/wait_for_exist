#include "test.h"
#include "tests.h"

int main(int argc, char *argv[]) {
    return test_main(argc, argv, (Test[]){
        GET_TEST(normpath_simple),
        GET_TEST(normpath_double_slash),
        GET_TEST(normpath_trailing_slash),
        GET_TEST(normpath_dot),
        GET_TEST(normpath_double_dot),
        GET_TEST(normpath_everything),
        GET_TEST(path_existing),
        GET_TEST(parent_exists),
        GET_TEST(long_path),
        GET_TEST(complex),
        { NULL, NULL },
    });
}
