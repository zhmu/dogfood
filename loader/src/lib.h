/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2023 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <cstddef>

void* memset(void* p, int c, size_t n);
extern "C" {
    void* memcpy(void* dst, const void* src, size_t len);
    void* memmove(void* dst, const void* src, size_t len);
}
size_t strlen(const char* src);
char* strchr(const char* s, int ch);
int strcmp(const char* a, const char* b);
int memcmp(const char* a, const char* b, size_t len);
[[noreturn]] void panic(const char* fmt, ...);

void assert_failure(const char* file, int line);

#define assert(expr) (expr) ? (void)0 : assert_failure(__FILE__, __LINE__)
