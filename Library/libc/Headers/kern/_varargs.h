//
//  _varargs.h
//  libsystem
//
//  Created by Dietmar Planitzer on 9/6/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __SYS_VARARGS_H
#define __SYS_VARARGS_H 1

#ifdef __SYSTEM_SHIM__
#include <stdarg.h>
#else

typedef unsigned char *va_list;

#define __va_align(type) (__alignof(type)>=4?__alignof(type):4)
#define __va_do_align(vl,type) ((vl)=(unsigned char *)((((unsigned int)(vl))+__va_align(type)-1)/__va_align(type)*__va_align(type)))
#define __va_mem(vl,type) (__va_do_align((vl),type),(vl)+=sizeof(type),((type*)(vl))[-1])

#define va_start(ap, lastarg) ((ap)=(va_list)(&lastarg+1))
#define va_arg(vl,type) __va_mem(vl,type)
#define va_copy(new,old) ((new)=(old))
#define va_end(vl) ((vl)=0)

#endif /* __SYSTEM_SHIM__ */
#endif /* __SYS_VARARGS_H */
