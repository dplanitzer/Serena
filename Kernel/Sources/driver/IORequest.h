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

struct IORequest;


typedef (*IODoneFunc)(void* ctx, struct IORequest* _Nonnull req);


typedef struct IOVector {
    uint8_t* _Nonnull   data;       // <- byte buffer to read or write 
    intptr_t            token;      // <- token identifying this disk block range
    ssize_t             size;       // <- request size in terms of bytes
} IOVector;


typedef struct IORequest {
    int                     type;           // <- request type
    uint16_t                size;           // <- request size in bytes
    uint16_t                status;         // -> request execution status
    IODoneFunc _Nullable    done;           // -> done callback (if async request)
    void* _Nullable         context;        // -> done callback context (if async request)
} IORequest;


// Returns an IORequest suitable for an async I/O call
extern errno_t IORequest_Get(int type, size_t reqSize, IORequest* _Nullable * _Nonnull pOutReq);
extern void IORequest_Put(IORequest* _Nullable req);

// Initializes an IORequest suitable for a sync I/O call
#define IORequest_Init(__req, __type) \
((IORequest*)__req)->type = (__type); \
((IORequest*)__req)->size = 0; \
((IORequest*)__req)->status = EOK; \
((IORequest*)__req)->done = NULL

#define IORequest_Done(__req) \
if (((IORequest*)__req)->done) {\
    ((IORequest*)__req)->done(((IORequest*)__req)->context, (IORequest*)__req);\
}

#endif /* IORequest_h */
