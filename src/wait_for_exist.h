/**
 * wait_for_exist  Copyright (C) 2026  Mathias Panzenböck
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef WAIT_FOR_EXIST_H__
#define WAIT_FOR_EXIST_H__
#pragma once

#include <time.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#define _WAIT_FOR_EXIST_STR2(X) #X
#define _WAIT_FOR_EXIST_STR(X) _WAIT_FOR_EXIST_STR2(X)

#define WAIT_FOR_EXIST_VERSION_MAJOR 1
#define WAIT_FOR_EXIST_VERSION_MINOR 0
#define WAIT_FOR_EXIST_VERSION_PATCH 0

#define WAIT_FOR_EXIST_VERSION \
    _WAIT_FOR_EXIST_STR(WAIT_FOR_EXIST_VERSION_MAJOR) "." \
    _WAIT_FOR_EXIST_STR(WAIT_FOR_EXIST_VERSION_MINOR) "." \
    _WAIT_FOR_EXIST_STR(WAIT_FOR_EXIST_VERSION_PATCH)

int wait_for_exist(const char *path, const struct timespec *timeout);

#ifdef __cplusplus
}
#endif

#endif
