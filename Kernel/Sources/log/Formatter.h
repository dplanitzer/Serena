//
//  Formatter.h
//  kernel
//
//  Created by Dietmar Planitzer on 1/23/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Formatter_h
#define Formatter_h

#include <klib/Error.h>
#include <klib/Types.h>

struct Formatter;


// Writes 'nbytes' bytes from 'pBuffer' to the sink.
typedef void (*Formatter_Sink)(struct Formatter* _Nonnull self, const char * _Nonnull pBuffer, ssize_t nBytes);

#define LENGTH_MODIFIER_hh      0
#define LENGTH_MODIFIER_h       1
#define LENGTH_MODIFIER_none    2
#define LENGTH_MODIFIER_l       3
#define LENGTH_MODIFIER_ll      4
#define LENGTH_MODIFIER_z       6

// <https://en.cppreference.com/w/c/io/fprintf>
struct ConversionSpec {
    int     minimumFieldWidth;
    int     precision;
    struct Flags {
        unsigned int isAlternativeForm:1;
        unsigned int padWithZeros:1;
        unsigned int hasPrecision:1;
    }       flags;
    int8_t    lengthModifier;
};


struct Formatter {
    Formatter_Sink _Nonnull sink;
    void* _Nullable         context;
    ssize_t                 charactersWritten;
    char                    digits[DIGIT_BUFFER_CAPACITY];
};


extern void Formatter_Init(struct Formatter* _Nonnull self, Formatter_Sink _Nonnull sink, void * _Nullable ctx);
extern void Formatter_vFormat(struct Formatter* _Nonnull self, const char* _Nonnull format, va_list ap);

#endif  /* Formatter_h */
