//
//  __scn.h
//  libc
//
//  Created by Dietmar Planitzer on 3/2/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef __EXT_SCN_H
#define __EXT_SCN_H

#include <_cmndef.h>
#include <arch/_ssize.h>
#include <arch/_size.h>
#include <stdarg.h>
#include <stdbool.h>
#include <__itoa.h>

__CPP_BEGIN

struct scn;
typedef struct scn scn_t;

struct scn_cspec;
typedef struct scn_cspec scn_cspec_t;


// Returns 1 and the next character from the stream 's' on success; 0 on EOF; -1 on error
typedef ssize_t (*scn_getc_t)(char* _Nonnull _Restrict pch, void * _Nonnull _Restrict s);

// Pushes the character 'c' back into the stream. Returns 'ch' on success and -1 otherwise
typedef int (*scn_ungetc_t)(int ch, void * _Nonnull s);


// Callback to scan the next vararg from 'ap' and write it to the stream
// associated with scanner 'self'. 'format' points to the character(s) that
// specifies how to scan the next argument. 'self->cspec' holds the scan
// parameters from teh format specifier.
typedef const char* _Nonnull (*scn_scan_t)(scn_t* _Nonnull _Restrict self, const char* _Nonnull _Restrict format, va_list* _Nonnull _Restrict ap);


#define SCN_LM_hh   0
#define SCN_LM_h    1
#define SCN_LM_none 2
#define SCN_LM_l    3
#define SCN_LM_ll   4
#define SCN_LM_j    5
#define SCN_LM_z    6
#define SCN_LM_t    7
#define SCN_LM_L    8

#define __SCN_SUPPRESSED    1

#define SCN_SUPPRESSED(x)   (((x) & __SCN_SUPPRESSED) == __SCN_SUPPRESSED)


// <https://en.cppreference.com/w/c/io/fscanf>
struct scn_cspec {
    int             max_field_width;
    char            lm;     // length modifier
    unsigned char   flags;
};


#define __SCN_HASERR    1   /* Syntax or I/O error */
#define __SCN_HASEOF    2

struct scn {
    void* _Nonnull          stream;
    scn_getc_t _Nonnull     getc_cb;
    scn_ungetc_t _Nonnull   ungetc_cb;
    scn_scan_t _Nullable    scan_cb;
    size_t                  chars_read;
    int                     fieldsAssigned;
    i64a_t                  i64a;
    scn_cspec_t             spec;
    unsigned char           flags;
};


// Initialize a scanner for int and pointer support only.
extern void __scn_init_i(scn_t* _Nonnull _Restrict self, void* _Nullable _Restrict s, scn_getc_t _Nonnull getc_f, scn_ungetc_t _Nonnull ungetc_f);

// Initialize a scanner for int, pointer and floating point support.
//extern void __scn_init_fp(scn_t* _Nonnull _Restrict self, void* _Nullable _Restrict s, scn_getc_func_t _Nonnull getc_f);

// Deinitialize the given scanner.
extern void __scn_deinit(scn_t* _Nullable self);


// Returns the number of fields assigned on success; -1 otherwise
extern int __scn_scan(scn_t* _Nonnull _Restrict self, const char* _Nonnull _Restrict format, va_list ap);


// Returns the next value of type 'ty' from the vararg list 'ap' and updates the
// list state accordingly.
#define scn_arg(ap, ty) va_arg(*(ap), ty)

// Marks the scanner as in error-state. Error here means either a syntax or I/O
// error.
#define scn_setfailed(self) \
(self)->flags |= __SCN_HASERR

// Marks the scanner as in eof-state.
#define scn_seteof(self) \
(self)->flags |= __SCN_HASEOF

// Returns true if the scanner is in error state.
#define scn_failed(self) \
((((self)->flags & __SCN_HASERR) == __SCN_HASERR) ? true : false)

// Returns true if the scanner is in eof state.
#define scn_eof(self) \
((((self)->flags & __SCN_HASEOF) == __SCN_HASEOF) ? true : false)


// Internal

#define __scn_common_init(self, s, getc_f, ungetc_f) \
(self)->stream = s; \
(self)->getc_cb = getc_f; \
(self)->ungetc_cb = ungetc_f; \
(self)->scan_cb = NULL; \
(self)->chars_read = 0; \
(self)->fieldsAssigned = 0; \
(self)->flags = 0;

__CPP_END

#endif  /* __EXT_SCN_H */
