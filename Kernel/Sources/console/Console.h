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

struct _Console;
typedef struct _Console* ConsoleRef;

extern ErrorCode Console_Create(EventDriverRef _Nonnull pEventDriver, GraphicsDriverRef _Nonnull pGDevice, ConsoleRef _Nullable * _Nonnull pOutConsole);
extern void Console_Destroy(ConsoleRef _Nullable pConsole);

extern ByteCount Console_Write(ConsoleRef _Nonnull pConsole, const Byte* _Nonnull pBytes, ByteCount nBytesToWrite);
extern ByteCount Console_Read(ConsoleRef _Nonnull pConsole, Byte* _Nonnull pBuffer, ByteCount nBytesToRead);

#endif /* Console_h */
