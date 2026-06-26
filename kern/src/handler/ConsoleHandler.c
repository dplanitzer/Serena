//
//  ConsoleHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "ConsoleHandler.h"
#include <console/Console.h>


errno_t ConsoleHandler_Create(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    return PseudoHandler_Create(class(ConsoleHandler), FD_TYPE_TERMINAL, ip, flags, pOutHandler);
}

errno_t ConsoleHandler_read(struct ConsoleHandler* _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    const fd_flags_t flags = Handler_GetFlags(self);

    if ((flags & O_RDONLY) != 0) {
        return Console_Read(gConsole, flags, pBuffer, nBytesToRead, nOutBytesRead);
    }
    else {
        return EBADF;
    }
}

errno_t ConsoleHandler_write(struct ConsoleHandler* _Nonnull _Locked self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    const fd_flags_t flags = Handler_GetFlags(self);

    if ((flags & O_WRONLY) != 0) {
        return Console_Write(gConsole, flags, pBuffer, nBytesToWrite, nOutBytesWritten);
    }
    else {
        return EBADF;
    }
}

errno_t ConsoleHandler_control(struct ConsoleHandler* _Nonnull self, int cmd, va_list ap)
{
    switch (cmd) {
        case IOCMD_TTY_SCREEN: {
            con_screen_t* scr = va_arg(ap, con_screen_t*);
            
            Console_GetScreenSize(gConsole, scr);
            return EOK;
        }

        case IOCMD_TTY_CURSOR: {
            con_cursor_t* crsr = va_arg(ap, con_cursor_t*);
            
            Console_GetCursorPosition(gConsole, crsr);
            return EOK;
        }

        default:
            return Handler_Super_Control(ConsoleHandler, cmd, ap);
    }
}


class_func_defs(ConsoleHandler, PseudoHandler,
override_func_def(read, ConsoleHandler, Handler)
override_func_def(write, ConsoleHandler, Handler)
override_func_def(control, ConsoleHandler, Handler)
);
