//
//  IORequest.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef IORequest_h
#define IORequest_h

#include <kern/errno.h>
#include <kern/types.h>
#include <dispatch/dispatch.h>

struct IORequest;


typedef struct IOVector {
    uint8_t* _Nonnull   data;       // <- byte buffer to read or write 
    intptr_t            token;      // <- token identifying this disk block range
    ssize_t             size;       // <- request size in terms of bytes
} IOVector;


typedef struct IORequest {
    struct dispatch_item    item;
    void* _Nonnull          driver;
    int                     type;           // <- request type
    uint16_t                size;           // <- request size in bytes
    uint16_t                status;         // -> request execution status
} IORequest;


// Returns an IORequest suitable for an async I/O call
extern errno_t IORequest_Get(int type, size_t reqSize, IORequest* _Nullable * _Nonnull pOutReq);
extern void IORequest_Put(IORequest* _Nullable req);

// Initializes an IORequest suitable for a sync I/O call
#define IORequest_Init(__req, __type) \
((IORequest*)__req)->type = (__type); \
((IORequest*)__req)->size = 0; \
((IORequest*)__req)->status = EOK; \
((IORequest*)__req)->item.retireFunc = NULL

#endif /* IORequest_h */
