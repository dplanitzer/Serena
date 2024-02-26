//
//  ShellContext.h
//  sh
//
//  Created by Dietmar Planitzer on 2/25/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef ShellContext_h
#define ShellContext_h

#include "LineReader.h"
#include <System/System.h>

typedef struct ShellContext {
    LineReaderRef _Weak lineReader;
} ShellContext;
typedef ShellContext* ShellContextRef;


extern errno_t ShellContext_Create(LineReaderRef _Nullable pLineReader, ShellContextRef _Nullable * _Nonnull pOutSelf);
extern void ShellContext_Destroy(ShellContextRef _Nullable self);

#endif  /* ShellContext_h */
