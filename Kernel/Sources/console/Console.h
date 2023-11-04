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

OPAQUE_CLASS(Console, IOResource);
typedef struct _ConsoleMethodTable {
    IOResourceMethodTable   super;
} ConsoleMethodTable;

extern ErrorCode Console_Create(EventDriverRef _Nonnull pEventDriver, GraphicsDriverRef _Nonnull pGDevice, ConsoleRef _Nullable * _Nonnull pOutConsole);

#endif /* Console_h */
