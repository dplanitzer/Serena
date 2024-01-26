//
//  Formatter.h
//  Apollo
//
//  Created by Dietmar Planitzer on 1/23/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Formatter_h
#define Formatter_h

#include "Types.h"
#include "Error.h"


struct Formatter;
typedef struct Formatter* FormatterRef;


// Writes 'nbytes' bytes from 'pBuffer' to the sink. Returns one of the EXX
// constants.
typedef ErrorCode (*Formatter_SinkFunc)(FormatterRef _Nonnull self, const Character * _Nonnull pBuffer, ByteCount nBytes);

#define LENGTH_MODIFIER_hh      0
#define LENGTH_MODIFIER_h       1
#define LENGTH_MODIFIER_none    2
#define LENGTH_MODIFIER_l       3
#define LENGTH_MODIFIER_ll      4
#define LENGTH_MODIFIER_z       6

// <https://en.cppreference.com/w/c/io/fprintf>
typedef struct ConversionSpec {
    Int     minimumFieldWidth;
    Int     precision;
    struct _Flags {
        unsigned int isLeftJustified:1;
        unsigned int alwaysShowSign:1;
        unsigned int showSpaceIfPositive:1;
        unsigned int isAlternativeForm:1;
        unsigned int padWithZeros:1;
        unsigned int hasPrecision:1;
    }       flags;
    Int8    lengthModifier;
} ConversionSpec;


typedef struct Formatter {
    Formatter_SinkFunc _Nonnull sink;
    void* _Nullable             context;
    ByteCount                   charactersWritten;
    ByteCount                   bufferCount;
    ByteCount                   bufferCapacity;
    Character* _Nonnull         buffer;
    Character                   digits[DIGIT_BUFFER_CAPACITY];
} Formatter;


extern void Formatter_Init(FormatterRef _Nonnull self, Formatter_SinkFunc _Nonnull pSinkFunc, void * _Nullable pContext, Character* _Nonnull pBuffer, Int bufferCapacity);

extern ErrorCode Formatter_vFormat(FormatterRef _Nonnull self, const Character* _Nonnull format, va_list ap);

#endif  /* Formatter_h */
