#pragma once

#include "types.h"

#define TS(x) _TS(x)
#define _TS(x) #x
#define _PANIC_LOCATION __FILE__ ":" TS(__LINE__)

#define assert(x) \
    ((x) ? (void)0 : panic("assertion failure: " _PANIC_LOCATION " condition: " TS(x)))

int printf(const char* fmt, ...);
void* memset(void* p, int c, size_t n);
void* memcpy(void* dst, const void* src, size_t len);
size_t strlcpy(char* dst, const char* src, size_t len);
char* strchr(const char* s, int ch);
int strcmp(const char* a, const char* b);
[[noreturn]] void panic(const char* s);
