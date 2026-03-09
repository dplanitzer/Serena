//
//  Asserts.h
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 1/30/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Asserts_h
#define Asserts_h 1

#include <_cmndef.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

__CPP_BEGIN

extern void Assert(const char* _Nonnull pFuncname, int lineNum, const char* _Nonnull fmt, ...);

#define assert_eof(cond)     if ((cond) != EOF) { Assert(__func__, __LINE__, "%s", #cond); }
#define assert_not_eof(cond)  if ((cond) == EOF) { Assert(__func__, __LINE__, "%s", #cond); }

#define assert_null(cond) if ((cond) != NULL) { Assert(__func__, __LINE__, "%s", #cond); }
#define assert_not_null(cond) if ((cond) == NULL) { Assert(__func__, __LINE__, "%s", #cond); }

#define assert_ok(cond) if ((cond) != 0) { Assert(__func__, __LINE__, "%s", #cond); }

#define assert_bool_eq(expected, actual) do { bool __tmp = (bool)(actual); if ((expected) != __tmp) { Assert(__func__, __LINE__, "" #expected " vs %s", (__tmp) ? "true" : "false"); } } while(0)
#define assert_char_eq(expected, actual) do { char __tmp = (char)(actual); if ((expected) != __tmp) { Assert(__func__, __LINE__, "" #expected " vs '%c'", __tmp); } } while(0)
#define assert_int_eq(expected, actual) do { int __tmp = (int)(actual); if ((expected) != __tmp) { Assert(__func__, __LINE__, "" #expected " vs %d", __tmp); } } while(0)
#define assert_uint_eq(expected, actual) do { unsigned int __tmp = (unsigned int)(actual); if ((expected) != __tmp) { Assert(__func__, __LINE__, "" #expected " vs %u", __tmp); } } while(0)
#define assert_size_eq(expected, actual) do { size_t __tmp = (size_t)(actual); if ((expected) != __tmp) { Assert(__func__, __LINE__, "" #expected " vs %z", __tmp); } } while(0)
#define assert_ssize_eq(expected, actual) do { ssize_t __tmp = (ssize_t)(actual); if ((expected) != __tmp) { Assert(__func__, __LINE__, "" #expected " vs %ld", __tmp); } } while(0)
#define assert_long_eq(expected, actual) do { long __tmp = (long)(actual); if ((expected) != __tmp) { Assert(__func__, __LINE__, "" #expected " vs %ld", __tmp); } } while(0)
#define assert_ulong_eq(expected, actual) do { unsigned long __tmp = (unsigned long)(actual); if ((expected) != __tmp) { Assert(__func__, __LINE__, "" #expected " vs %lu", __tmp); } } while(0)
#define assert_long_long_eq(expected, actual) do { long long __tmp = (long long)(actual); if ((expected) != __tmp) { Assert(__func__, __LINE__, "" #expected " vs %lld", __tmp); } } while(0)
#define assert_ulong_long_eq(expected, actual) do { unsigned long long __tmp = (unsigned long long)(actual); if ((expected) != __tmp) { Assert(__func__, __LINE__, "" #expected " vs %llu", __tmp); } } while(0)
#define assert_double_eq(expected, actual) do { double __tmp = (double)(actual); if ((expected) != __tmp) { Assert(__func__, __LINE__, "" #expected " vs %g", __tmp); } } while(0)
#define assert_ptr_eq(expected, actual) do { void* __tmp = (void*)(actual); if ((expected) != __tmp) { Assert(__func__, __LINE__, "" #expected " vs %p", __tmp); } } while(0)

#define assert_int_ge(expected, actual) do { int __tmp = (int)(actual); if ((expected) > __tmp) { Assert(__func__, __LINE__, "" #expected " vs %d", __tmp); } } while(0)
#define assert_ssize_ge(expected, actual) do { ssize_t __tmp = (ssize_t)(actual); if ((expected) > __tmp) { Assert(__func__, __LINE__, "" #expected " vs %ld", __tmp); } } while(0)

#define assert_true(actual) if (!(actual)) { Assert(__func__, __LINE__, "%s", #actual); }
#define assert_false(actual) if (actual) { Assert(__func__, __LINE__, "%s", #actual); }

__CPP_END

#endif /* Asserts_h */
