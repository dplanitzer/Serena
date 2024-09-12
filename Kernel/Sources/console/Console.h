//
//  Console.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Console_h
#define Console_h

#include <klib/klib.h>
#include <driver/Driver.h>
#include <driver/amiga/graphics/GraphicsDriver.h>
#include <driver/hid/EventDriver.h>

//
// The console implements support for the following standards:
// DEC VT52                                         <https://vt100.net/dec/ek-vt5x-op-001.pdf>
// DEC VT52 Atari Extensions                        <https://en.wikipedia.org/wiki/VT52#GEMDOS/TOS_extensions>
// DEC VT100                                        <https://vt100.net/docs/vt100-ug/contents.html>
// DEC VT102 (ANSI X3.41-1977 & ANSI X3.64-1979)    <https://vt100.net/docs/vt102-ug/contents.html>
//
final_class(Console, Driver);

extern errno_t Console_Create(EventDriverRef _Nonnull pEventDriver, GraphicsDriverRef _Nonnull pGDevice, ConsoleRef _Nullable * _Nonnull pOutConsole);

#endif /* Console_h */
