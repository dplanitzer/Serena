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
typedef struct Formatter* FormatterRef;


// Writes 'nbytes' bytes from 'pBuffer' to the sink. Returns one of the EXX
// constants.
typedef errno_t (*Formatter_SinkFunc)(FormatterRef _Nonnull self, const char * _Nonnull pBuffer, ssize_t nBytes);

#define LENGTH_MODIFIER_hh      0
#define LENGTH_MODIFIER_h       1
#define LENGTH_MODIFIER_none    2
#define LENGTH_MODIFIER_l       3
#define LENGTH_MODIFIER_ll      4
#define LENGTH_MODIFIER_z       6

// <https://en.cppreference.com/w/c/io/fprintf>
typedef struct ConversionSpec {
    int     minimumFieldWidth;
    int     precision;
    struct Flags {
        unsigned int isAlternativeForm:1;
        unsigned int padWithZeros:1;
        unsigned int hasPrecision:1;
    }       flags;
    int8_t    lengthModifier;
} ConversionSpec;


typedef struct Formatter {
    Formatter_SinkFunc _Nonnull sink;
    void* _Nullable             context;
    ssize_t                     charactersWritten;
    ssize_t                     bufferCount;
    ssize_t                     bufferCapacity;
    char* _Nonnull              buffer;
    char                        digits[DIGIT_BUFFER_CAPACITY];
} Formatter;


extern void Formatter_Init(FormatterRef _Nonnull self, Formatter_SinkFunc _Nonnull pSinkFunc, void * _Nullable pContext, char* _Nonnull pBuffer, int bufferCapacity);

extern errno_t Formatter_vFormat(FormatterRef _Nonnull self, const char* _Nonnull format, va_list ap);

#endif  /* Formatter_h */
