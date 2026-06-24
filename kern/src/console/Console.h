//
//  Console.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Console_h
#define Console_h

#include <kobj/Object.h>
#include <kpi/console.h>
#include <kpi/types.h>

//
// The console implements support for the following standards:
// DEC VT52                                         <https://vt100.net/dec/ek-vt5x-op-001.pdf>
// DEC VT52 Atari Extensions                        <https://en.wikipedia.org/wiki/VT52#GEMDOS/TOS_extensions>
// DEC VT100                                        <https://vt100.net/docs/vt100-ug/contents.html>
// DEC VT102 (ANSI X3.41-1977 & ANSI X3.64-1979)    <https://vt100.net/docs/vt102-ug/contents.html>
//
final_class(Console, Object);

extern ConsoleRef gConsole;

extern errno_t Console_Create(ConsoleRef _Nullable * _Nonnull pOutSelf);

extern void Console_Start(ConsoleRef _Nonnull self);

extern void Console_GetScreenSize(ConsoleRef _Nonnull self, con_screen_t* _Nonnull scr);
extern void Console_GetCursorPosition(ConsoleRef _Nonnull self, con_cursor_t* _Nonnull crsr);

extern errno_t Console_Read(ConsoleRef _Nonnull self, fd_flags_t flags, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead);
extern errno_t Console_Write(ConsoleRef _Nonnull self, fd_flags_t flags, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten);

#endif /* Console_h */
