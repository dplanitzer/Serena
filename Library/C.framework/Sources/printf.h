//
//  printf.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _PRINTF_H
#define _PRINTF_H 1

#include <__stddef.h>

extern const char* _Nonnull __lltoa(int64_t val, int base, bool isUppercase, int fieldWidth, char paddingChar, char* _Nonnull pString, size_t maxLength);
extern const char* _Nonnull __ulltoa(uint64_t val, int base, bool isUppercase, int fieldWidth, char paddingChar, char* _Nonnull pString, size_t maxLength);


struct _CharacterStream;
typedef struct _CharacterStream CharacterStream;

// Writes 'nbytes' bytes from 'pBuffer' to the sink. Returns one of the EXX
// constants.
typedef errno_t (* _Nonnull PrintSink_Func)(CharacterStream* _Nonnull pStream, const char * _Nonnull pBuffer, size_t nBytes);


#define STREAM_BUFFER_CAPACITY 64
struct _CharacterStream {
    PrintSink_Func _Nonnull sinkFunc;
    void* _Nullable         context;
    size_t                  charactersWritten;
    size_t                  bufferCount;
    size_t                  bufferCapacity;
    char                    buffer[STREAM_BUFFER_CAPACITY];
};


extern void __vprintf_init(CharacterStream* _Nonnull pStream, PrintSink_Func _Nonnull pSinkFunc, void* _Nullable pContext);
extern errno_t __vprintf(CharacterStream* _Nonnull pStream, const char* _Nonnull format, va_list ap);

#endif /* _PRINTF_H */
