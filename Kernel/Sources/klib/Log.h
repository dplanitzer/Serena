//
//  Log.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Log_h
#define Log_h

#include <klib/Types.h>

extern void print_init(void);
extern void print(const Character* _Nonnull format, ...);
extern void printv(const Character* _Nonnull format, va_list ap);

typedef void (* _Nonnull PrintSink_Func)(void* _Nullable pContext, const Character* _Nonnull pString);

extern void _printv(PrintSink_Func _Nonnull pSinkFunc, void* _Nullable pContext, Character* _Nonnull pBuffer, Int bufferCapacity, const Character* _Nonnull format, va_list ap);

#endif /* Log_h */
