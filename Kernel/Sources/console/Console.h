//
//  Console.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Console_h
#define Console_h

#include <klib/klib.h>
#include "EventDriver.h"
#include "GraphicsDriver.h"

//
// The console implements support for the following standards:
// DEC VT100                                        <https://vt100.net/docs/vt100-ug/contents.html>
// DEC VT102 (ANSI X3.41-1977 & ANSI X3.64-1979)    <https://vt100.net/docs/vt102-ug/contents.html>
//
OPAQUE_CLASS(Console, IOResource);
typedef struct _ConsoleMethodTable {
    IOResourceMethodTable   super;
} ConsoleMethodTable;

extern errno_t Console_Create(EventDriverRef _Nonnull pEventDriver, GraphicsDriverRef _Nonnull pGDevice, ConsoleRef _Nullable * _Nonnull pOutConsole);

#endif /* Console_h */
