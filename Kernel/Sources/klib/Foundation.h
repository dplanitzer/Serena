//
//  Foundation.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Foundation_h
#define Foundation_h

#include <klib/Types.h>


#define SIZE_GB(x)  ((UInt)(x) * 1024ul * 1024ul * 1024ul)
#define SIZE_MB(x)  ((Int)(x) * 1024 * 1024)
#define SIZE_KB(x)  ((Int)(x) * 1024)


// Variable argument lists
typedef unsigned char *va_list;

#define __va_align(type) (__alignof(type)>=4?__alignof(type):4)
#define __va_do_align(vl,type) ((vl)=(unsigned char *)((((unsigned int)(vl))+__va_align(type)-1)/__va_align(type)*__va_align(type)))
#define __va_mem(vl,type) (__va_do_align((vl),type),(vl)+=sizeof(type),((type*)(vl))[-1])

#define va_start(ap, lastarg) ((ap)=(va_list)(&lastarg+1))
#define va_arg(vl,type) __va_mem(vl,type)
#define va_end(vl) ((vl)=0)
#define va_copy(new,old) ((new)=(old))


// A callback function that takes a single (context) pointer argument
typedef void (* _Nonnull Closure1Arg_Func)(Byte* _Nullable pContext);


// Printing
extern void print_init(void);
extern void print(const Character* _Nonnull format, ...);
extern void printv(const Character* _Nonnull format, va_list ap);

typedef void (* _Nonnull PrintSink_Func)(void* _Nullable pContext, const Character* _Nonnull pString);

extern void _printv(PrintSink_Func _Nonnull pSinkFunc, void* _Nullable pContext, Character* _Nonnull pBuffer, Int bufferCapacity, const Character* _Nonnull format, va_list ap);

#endif /* Foundation_h */
