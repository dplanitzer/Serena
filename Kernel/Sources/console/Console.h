//
//  Console.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Console_h
#define Console_h

#include <handler/Handler.h>

//
// The console implements support for the following standards:
// DEC VT52                                         <https://vt100.net/dec/ek-vt5x-op-001.pdf>
// DEC VT52 Atari Extensions                        <https://en.wikipedia.org/wiki/VT52#GEMDOS/TOS_extensions>
// DEC VT100                                        <https://vt100.net/docs/vt100-ug/contents.html>
// DEC VT102 (ANSI X3.41-1977 & ANSI X3.64-1979)    <https://vt100.net/docs/vt102-ug/contents.html>
//
final_class(Console, Handler);

extern errno_t Console_Create(ConsoleRef _Nullable * _Nonnull pOutSelf);

extern void Console_Start(ConsoleRef _Nonnull self);

#endif /* Console_h */
