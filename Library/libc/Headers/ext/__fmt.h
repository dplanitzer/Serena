//
//  __fmt.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 1/23/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef __EXT_FMT_H
#define __EXT_FMT_H

#include <_cmndef.h>
#include <arch/_ssize.h>
#include <arch/_size.h>
#include <stdarg.h>
#include <stdbool.h>
#include <__itoa.h>

__CPP_BEGIN

// Writes character 'ch' to stream 's'. Returns 1 on success and <= 0 otherwise.
typedef ssize_t (*fmt_putc_func_t)(char ch, void* _Nullable s);

// Writes the first 'nbytes' bytes from 'buffer' to stream 's'. Returns 'nbytes'
// on success; <= 0 otherwise.
typedef ssize_t (*fmt_write_func_t)(void* _Nullable _Restrict s, const void * _Restrict buffer, ssize_t nbytes);


#define FMT_LENMOD_hh   0
#define FMT_LENMOD_h    1
#define FMT_LENMOD_none 2
#define FMT_LENMOD_l    3
#define FMT_LENMOD_ll   4
#define FMT_LENMOD_j    5
#define FMT_LENMOD_z    6
#define FMT_LENMOD_t    7
#define FMT_LENMOD_L    8

#define __FMT_LEFTJUST      1
#define __FMT_FORCESIGN     2
#define __FMT_SPACEIFPOS    4
#define __FMT_ALTFORM       8
#define __FMT_PADZEROS      16
#define __FMT_HASPREC       32


// <https://en.cppreference.com/w/c/io/fprintf>
typedef struct fmt_cspec {
    int             minFieldWidth;
    int             prec;
    char            lenMod;
    unsigned char   flags;
} fmt_cspec_t;


#define __FMT_HASERR        1
#define __FMT_CONTCNTONERR  2

typedef struct fmt {
    void* _Nonnull              stream;
    fmt_putc_func_t _Nonnull    putc_cb;
    fmt_write_func_t _Nonnull   write_cb;
    size_t                      charactersWritten;
    i64a_t                      i64a;
    fmt_cspec_t                 spec;
    unsigned char               flags;
} fmt_t;


extern void __fmt_init(fmt_t* _Nonnull _Restrict self, void* _Nullable _Restrict s, fmt_putc_func_t _Nonnull putc_f, fmt_write_func_t _Nonnull write_f, bool doContCountingOnError);
extern void __fmt_deinit(fmt_t* _Nullable self);

// Returns the number of characters written on success; -1 otherwise
extern int __fmt_format(fmt_t* _Nonnull _Restrict self, const char* _Nonnull _Restrict format, va_list ap);

__CPP_END

#endif  /* __EXT_FMT_H */
