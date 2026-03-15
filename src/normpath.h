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
#ifndef NORMPATH_H__
#define NORMPATH_H__
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

char *normpath(const char *path);

#ifdef __cplusplus
}
#endif

#endif
