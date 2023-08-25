//
//  printf.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _PRINTF_H
#define _PRINTF_H 1

#include <stdarg.h>

#ifndef __has_feature
#define __has_feature(x) 0
#endif

#if !__has_feature(nullability)
#ifndef _Nullable
#define _Nullable
#endif
#ifndef _Nonnull
#define _Nonnull
#endif
#endif

extern const char *__lltoa(int64_t val, int base, int fieldWidth, char paddingChar, char *pString, size_t maxLength);
extern const char *__ulltoa(uint64_t val, int base, int fieldWidth, char paddingChar, char *pString, size_t maxLength);


typedef int (* _Nonnull PrintSink_Func)(void * _Nullable pContext, const char * _Nonnull pString);

extern int __vprintf(PrintSink_Func _Nonnull pSinkFunc, void * _Nullable pContext, const char * _Nonnull format, va_list ap);

#endif /* _PRINTF_H */
