//
//  Log.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Log_h
#define Log_h

#include <klib/Types.h>

extern void print_init(void);
extern void printf(const char* _Nonnull format, ...);
extern void vprintf(const char* _Nonnull format, va_list ap);

#endif /* Log_h */
