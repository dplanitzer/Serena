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


typedef struct IOVector {
    uint8_t* _Nonnull   iov_base;       // <- byte buffer to read or write 
    intptr_t            iov_token;      // <- token identifying this disk block range
    ssize_t             iov_len;        // <- request size in terms of bytes
} IOVector;


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


// Returns an IODiskCommand suitable for an async I/O call
extern errno_t IODiskCommand_Get(size_t iopSize, IODiskCommand* _Nullable * _Nonnull pOutIop);
extern void IODiskCommand_Put(IODiskCommand* _Nullable iop);

// Initializes an IODiskCommand
#define IODiskCommand_Init(__iop, __type) \
((IODiskCommand*)(__iop))->u.item = KDISPATCH_ITEM_INIT(NULL, NULL); \
((IODiskCommand*)(__iop))->type = (__type); \
((IODiskCommand*)(__iop))->status = EOK;

#endif /* IODiskCommand_h */
