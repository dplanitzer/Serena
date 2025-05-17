//
//  kpi/fcntl.h
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_FCNTL_H
#define _KPI_FCNTL_H 1

#define O_RDONLY    0x0001
#define O_WRONLY    0x0002
#define O_RDWR      (O_RDONLY | O_WRONLY)
#define O_APPEND    0x0004
#define O_EXCL      0x0008
#define O_TRUNC     0x0010
#define O_NONBLOCK  0x0020


#define IOResourceCommand(__cmd) (__cmd)
#define IOChannelCommand(__cmd) -(__cmd)
#define IsIOChannelCommand(__cmd) ((__cmd) < 0)

// Returns the type of an I/O channel. The type indicates to which kind of
// I/O resource the channel is connected and thus which kind of operations are
// supported by the channel.
#define kIOChannelCommand_GetType   IOChannelCommand(1)
typedef enum IOChannelType {
    kIOChannelType_Terminal,
    kIOChannelType_File,
    kIOChannelType_Directory,
    kIOChannelType_Pipe,
    kIOChannelType_Driver,
    kIOChannelType_Filesystem,
    kIOChannelType_Process,
} IOChannelType;

// Returns the mode with which the I/O channel was opened.
// unsigned int get_mode(int ioc)
#define kIOChannelCommand_GetMode   IOChannelCommand(2)

// Updates the mode of an I/O channel. Enables 'mode' on the channel if 'setOrClear'
// is != 0 and disables 'mode' if 'setOrClear' == 0.
// The following modes may be changed:
// - O_APPEND
// - O_NONBLOCK
// errno_t set_mode(int ioc, int setOrClear, unsigned int mode)
#define kIOChannelCommand_SetMode   IOChannelCommand(3)

#endif /* _KPI_FCNTL_H */
