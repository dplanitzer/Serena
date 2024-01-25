//
//  Formatter.h
//  libc
//
//  Created by Dietmar Planitzer on 1/23/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Formatter_h
#define Formatter_h

#include <__stddef.h>


struct Formatter;
typedef struct Formatter* FormatterRef;


// Writes 'nbytes' bytes from 'pBuffer' to the sink. Returns one of the EXX
// constants.
typedef errno_t (*Formatter_SinkFunc)(FormatterRef _Nonnull self, const char * _Nonnull pBuffer, size_t nBytes);

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
    struct _Flags {
        unsigned int isLeftJustified:1;
        unsigned int alwaysShowSign:1;
        unsigned int showSpaceIfPositive:1;
        unsigned int isAlternativeForm:1;
        unsigned int padWithZeros:1;
        unsigned int hasPrecision:1;
    }       flags;
    char    lengthModifier;
} ConversionSpec;


#define FORMATTER_BUFFER_CAPACITY 64

typedef struct Formatter {
    Formatter_SinkFunc _Nonnull sink;
    void* _Nullable             context;
    size_t                      charactersWritten;
    size_t                      bufferCount;
    size_t                      bufferCapacity;
    char                        buffer[FORMATTER_BUFFER_CAPACITY];
    char                        digits[DIGIT_BUFFER_CAPACITY];
} Formatter;


extern errno_t Formatter_Init(FormatterRef _Nonnull self, Formatter_SinkFunc _Nonnull pSinkFunc, void * _Nullable pContext);
extern void Formatter_Deinit(FormatterRef _Nullable self);

extern errno_t Formatter_Format(FormatterRef _Nonnull self, const char* _Nonnull format, ...);
extern errno_t Formatter_vFormat(FormatterRef _Nonnull self, const char* _Nonnull format, va_list ap);

#endif  /* Formatter_h */
