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

struct fmt;
typedef struct fmt fmt_t;

struct fmt_cspec;
typedef struct fmt_cspec fmt_cspec_t;


// Writes character 'ch' to stream 's'. Returns 1 on success and <= 0 otherwise.
typedef ssize_t (*fmt_putc_func_t)(char ch, void* _Nullable s);

// Writes the first 'nbytes' bytes from 'buffer' to stream 's'. Returns 'nbytes'
// on success; <= 0 otherwise.
typedef ssize_t (*fmt_write_func_t)(void* _Nullable _Restrict s, const void * _Restrict buffer, ssize_t nbytes);


// Callback to format the next vararg from 'ap' and write it to the stream
// associated with formatter 'self'.
typedef void (*fmt_format_func_t)(fmt_t* _Nonnull _Restrict self, char conversion, va_list* _Nonnull _Restrict ap);


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

#define FMT_IS_LEFTJUST(x)      (((x) & __FMT_LEFTJUST) == __FMT_LEFTJUST)
#define FMT_IS_FORCESIGN(x)     (((x) & __FMT_FORCESIGN) == __FMT_FORCESIGN)
#define FMT_IS_SPACEIFPOS(x)    (((x) & __FMT_SPACEIFPOS) == __FMT_SPACEIFPOS)
#define FMT_IS_ALTFORM(x)       (((x) & __FMT_ALTFORM) == __FMT_ALTFORM)
#define FMT_IS_PADZEROS(x)      (((x) & __FMT_PADZEROS) == __FMT_PADZEROS)
#define FMT_IS_HASPREC(x)       (((x) & __FMT_HASPREC) == __FMT_HASPREC)


// <https://en.cppreference.com/w/c/io/fprintf>
struct fmt_cspec {
    int             minFieldWidth;
    int             prec;
    char            lenMod;
    unsigned char   flags;
};


#define __FMT_HASERR        1
#define __FMT_CONTCNTONERR  2

struct fmt {
    void* _Nonnull              stream;
    fmt_putc_func_t _Nonnull    putc_cb;
    fmt_write_func_t _Nonnull   write_cb;
    fmt_format_func_t _Nullable format_cb;
    size_t                      charactersWritten;
    i64a_t                      i64a;
    fmt_cspec_t                 spec;
    unsigned char               flags;
};


// Initialize a formatter for int and pointer support only.
extern void __fmt_init_i(fmt_t* _Nonnull _Restrict self, void* _Nullable _Restrict s, fmt_putc_func_t _Nonnull putc_f, fmt_write_func_t _Nonnull write_f, bool doContCountingOnError);

#if ___STDC_HOSTED__ == 1
// Initialize a formatter for int, pointer and floating point support.
extern void __fmt_init_fp(fmt_t* _Nonnull _Restrict self, void* _Nullable _Restrict s, fmt_putc_func_t _Nonnull putc_f, fmt_write_func_t _Nonnull write_f, bool doContCountingOnError);
#endif

// Deinitialize the given formatter.
extern void __fmt_deinit(fmt_t* _Nullable self);


// Returns the number of characters written on success; -1 otherwise
extern int __fmt_format(fmt_t* _Nonnull _Restrict self, const char* _Nonnull _Restrict format, va_list ap);


// Returns the next value of type 'ty' from the vararg list 'ap' and updates the
// list state accordingly.
#define fmt_arg(ap, ty) va_arg(*(ap), ty)

// Functions that X can use to write characters and strings to the stream
// attached to the formatter.
extern void __fmt_write_char(fmt_t* _Nonnull self, char ch);
extern void __fmt_write_string(fmt_t *_Nonnull _Restrict self, const char* _Nonnull _Restrict str, ssize_t len);
extern void __fmt_write_char_rep(fmt_t* _Nonnull self, char ch, int count);


// Internal

#define __fmt_common_init(self, s, putc_f, write_f, doContCountingOnError) \
(self)->stream = s; \
(self)->putc_cb = putc_f; \
(self)->write_cb = write_f; \
(self)->charactersWritten = 0; \
(self)->flags = 0; \
if (doContCountingOnError) { \
    (self)->flags |= __FMT_CONTCNTONERR; \
}

__CPP_END

#endif  /* __EXT_FMT_H */
