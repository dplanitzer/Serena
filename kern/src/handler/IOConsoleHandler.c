//
//  IOConsoleHandler.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "IOConsoleHandler.h"
#include <console/Console.h>


errno_t IOConsoleHandler_Create(ConsoleRef _Nonnull con, fd_flags_t flags, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    return IODriverHandler_Create(class(IOConsoleHandler), FD_TYPE_TERMINAL, flags, (DriverRef)con, pOutHandler);
}

errno_t IOConsoleHandler_read(struct IOConsoleHandler* _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    const fd_flags_t flags = Handler_GetFlags(self);
    ConsoleRef con = IODriverHandler_GetDriver(self);

    if ((flags & O_RDONLY) != 0) {
        return Driver_Read(con, flags, NULL, pBuffer, nBytesToRead, nOutBytesRead);
    }
    else {
        return EBADF;
    }
}

errno_t IOConsoleHandler_write(struct IOConsoleHandler* _Nonnull _Locked self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    const fd_flags_t flags = Handler_GetFlags(self);
    ConsoleRef con = IODriverHandler_GetDriver(self);

    if ((flags & O_WRONLY) != 0) {
        return Driver_Write(con, flags, NULL, pBuffer, nBytesToWrite, nOutBytesWritten);
    }
    else {
        return EBADF;
    }
}

errno_t IOConsoleHandler_control(struct IOConsoleHandler* _Nonnull self, int cmd, va_list ap)
{
    ConsoleRef con = IODriverHandler_GetDriver(self);

    switch (cmd) {
        case kConsoleCommand_GetScreen: {
            con_screen_t* scr = va_arg(ap, con_screen_t*);
            
            Console_GetScreenSize(con, scr);
            return EOK;
        }

        case kConsoleCommand_GetCursor: {
            con_cursor_t* crsr = va_arg(ap, con_cursor_t*);
            
            Console_GetCursorPosition(con, crsr);
            return EOK;
        }

        default:
            return Handler_Super_Control(IOConsoleHandler, cmd, ap);
    }
}


class_func_defs(IOConsoleHandler, IODriverHandler,
override_func_def(read, IOConsoleHandler, Handler)
override_func_def(write, IOConsoleHandler, Handler)
override_func_def(control, IOConsoleHandler, Handler)
);
