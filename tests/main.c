#include "test.h"
#include "tests.h"

int main(int argc, char *argv[]) {
    return test_main(argc, argv, (Test[]){
        test_case_path_existing,
        test_case_parent_exists,
        test_case_long_path,
        test_case_complex,
        { NULL, NULL },
    });
}
