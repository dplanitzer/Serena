//
//  IORequest.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef IORequest_h
#define IORequest_h

#include <stdint.h>
#include <stddef.h>
#include <ext/try.h>
#include <kdispatch/kdispatch.h>

struct IORequest;


typedef struct IOVector {
    uint8_t* _Nonnull   iov_base;       // <- byte buffer to read or write 
    intptr_t            iov_token;      // <- token identifying this disk block range
    ssize_t             iov_len;        // <- request size in terms of bytes
} IOVector;


typedef struct IORequest {
    union {
        struct kdispatch_item   item;
        queue_node_t            qe;
    }                       u;
    size_t                  size;
    void* _Nonnull          driver;
    int                     type;           // <- request type
    errno_t                 status;         // -> request execution status
} IORequest;


// Returns an IORequest suitable for an async I/O call
extern errno_t IORequest_Get(size_t reqSize, IORequest* _Nullable * _Nonnull pOutReq);
extern void IORequest_Put(IORequest* _Nullable req);

// Initializes an IORequest suitable for a sync I/O call
#define IORequest_Init(__req, __type) \
((IORequest*)__req)->u.item = KDISPATCH_ITEM_INIT(NULL, NULL); \
((IORequest*)__req)->type = (__type); \
((IORequest*)__req)->status = EOK;

#endif /* IORequest_h */
