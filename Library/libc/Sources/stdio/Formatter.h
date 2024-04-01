//
//  Formatter.h
//  libc
//
//  Created by Dietmar Planitzer on 1/23/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Formatter_h
#define Formatter_h

#include <stdio.h>
#include <__stddef.h>


struct Formatter;
typedef struct Formatter* FormatterRef;


#define LENGTH_MODIFIER_hh      0
#define LENGTH_MODIFIER_h       1
#define LENGTH_MODIFIER_none    2
#define LENGTH_MODIFIER_l       3
#define LENGTH_MODIFIER_ll      4
#define LENGTH_MODIFIER_j       5
#define LENGTH_MODIFIER_z       6
#define LENGTH_MODIFIER_t       7
#define LENGTH_MODIFIER_L       8


// <https://en.cppreference.com/w/c/io/fprintf>
typedef struct ConversionSpec {
    int     minimumFieldWidth;
    int     precision;
    struct Flags {
        unsigned int isLeftJustified:1;
        unsigned int alwaysShowSign:1;
        unsigned int showSpaceIfPositive:1;
        unsigned int isAlternativeForm:1;
        unsigned int padWithZeros:1;
        unsigned int hasPrecision:1;
    }       flags;
    char    lengthModifier;
} ConversionSpec;


typedef struct Formatter {
    FILE* _Nonnull  stream;
    size_t          charactersWritten;
    char            digits[DIGIT_BUFFER_CAPACITY];
} Formatter;


static inline void __Formatter_Init(FormatterRef _Nonnull self, FILE* _Nonnull pStream) {
    self->stream = pStream;
    self->charactersWritten = 0;
}

static inline void __Formatter_Deinit(FormatterRef _Nullable self) {
    self->stream = NULL;
}

extern errno_t __Formatter_vFormat(FormatterRef _Nonnull self, const char* _Nonnull format, va_list ap);

#endif  /* Formatter_h */
