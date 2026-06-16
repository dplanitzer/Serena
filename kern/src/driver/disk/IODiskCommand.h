//
//  IODiskCommand.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef IODiskCommand_h
#define IODiskCommand_h

#include <stdint.h>
#include <stddef.h>
#include <ext/try.h>
#include <kdispatch/kdispatch.h>
#include <kpi/disk.h>


typedef struct IOVector {
    uint8_t* _Nonnull   iov_base;       // <- byte buffer to read or write 
    intptr_t            iov_token;      // <- token identifying this disk block range
    ssize_t             iov_len;        // <- request size in terms of bytes
} IOVector;


enum {
    kIODiskCommand_Read = 1,
    kIODiskCommand_Write,
    kIODiskCommand_FormatDisk,
    kIODiskCommand_FormatTrack,
    kIODiskCommand_GetDriveInfo,
    kIODiskCommand_GetDiskInfo,
    kIODiskCommand_SenseDisk,
};


typedef struct IODiskCommand {
    union {
        struct kdispatch_item   item;
        queue_node_t            qe;
    }                       u;
    size_t                  size;
    void* _Nonnull          driver;
    int                     type;           // <- request type
    errno_t                 status;         // -> request execution status
} IODiskCommand;


typedef struct IORWCommand {
    IODiskCommand       s;
    off_t               offset;         // <- logical sector address in terms of bytes
    unsigned int        options;        // <- read/write options
    ssize_t             resCount;       // -> number of bytes read/written
    size_t              iovCount;       // <- number of I/O vectors in this request

    IOVector            iov[1];
} IORWCommand;


typedef struct IOFormatDiskCommand {
    IODiskCommand   s;
    char            fillByte;   // <- data for all sectors in the cluster to format
} IOFormatDiskCommand;


typedef struct IOFormatTrackCommand {
    IODiskCommand   s;
    off_t           offset;     // <- logical sector address in terms of bytes
    char            fillByte;   // <- data for all sectors in the cluster to format
    ssize_t         rlen;       // -> number of bytes formatted
} IOFormatTrackCommand;


typedef struct IOGetDriveInfoCommand {
    IODiskCommand           s;
    drive_info_t* _Nonnull  ip;
} IOGetDriveInfoCommand;


typedef struct IOGetDiskInfoCommand {
    IODiskCommand           s;
    disk_info_t* _Nonnull   gp;
} IOGetDiskInfoCommand;


typedef struct IOSenseDiskCommand {
    IODiskCommand   s;
} IOSenseDiskCommand;


// Returns an IODiskCommand suitable for an async I/O call
extern errno_t IODiskCommand_Get(size_t iopSize, IODiskCommand* _Nullable * _Nonnull pOutIop);
extern void IODiskCommand_Put(IODiskCommand* _Nullable iop);

// Initializes an IODiskCommand
#define IODiskCommand_Init(__iop, __type) \
((IODiskCommand*)(__iop))->u.item = KDISPATCH_ITEM_INIT(NULL, NULL); \
((IODiskCommand*)(__iop))->type = (__type); \
((IODiskCommand*)(__iop))->status = EOK;

#endif /* IODiskCommand_h */
