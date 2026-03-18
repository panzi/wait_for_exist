#ifndef TESTS_H
#define TESTS_H
#pragma once

#include "test.h"

#ifdef __cplusplus
extern "C" {
#endif

DECL_TEST(normpath_simple);
DECL_TEST(normpath_double_slash);
DECL_TEST(normpath_trailing_slash);
DECL_TEST(normpath_dot);
DECL_TEST(normpath_double_dot);
DECL_TEST(normpath_everything);

DECL_TEST(path_existing);
DECL_TEST(parent_exists);
DECL_TEST(long_path);
DECL_TEST(complex);

#ifdef __cplusplus
}
#endif

#endif
