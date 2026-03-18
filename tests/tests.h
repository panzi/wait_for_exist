#ifndef TESTS_H
#define TESTS_H
#pragma once

#include "test.h"

#ifdef __cplusplus
extern "C" {
#endif

DECL_TEST(path_existing);
DECL_TEST(parent_exists);
DECL_TEST(long_path);
DECL_TEST(complex);

#ifdef __cplusplus
}
#endif

#endif
