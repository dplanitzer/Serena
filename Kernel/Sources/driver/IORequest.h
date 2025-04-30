//
//  IORequest.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/28/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef IORequest_h
#define IORequest_h

#include <klib/Error.h>
#include <klib/Types.h>

struct IORequest;


typedef (*IODoneFunc)(void* ctx, struct IORequest* _Nonnull req);


typedef struct IOVector {
    uint8_t* _Nonnull   data;       // -> byte buffer to read or write 
    intptr_t            token;      // -> token identifying this disk block range
    ssize_t             size;       // -> request size in terms of bytes
} IOVector;


typedef struct IORequest {
    int                     type;           // -> request type
    uint16_t                size;           // -> request size in bytes
    uint16_t                status;         // <- request execution status
    IODoneFunc _Nullable    done;           // <- done callback (if async request)
    void* _Nullable         context;        // <- done callback context (if async request)
} IORequest;


extern errno_t IORequest_Get(int type, size_t reqSize, IORequest* _Nullable * _Nonnull pOutReq);
extern void IORequest_Put(IORequest* _Nullable req);

#define IORequest_Done(__req) \
if (((IORequest*)__req)->done) {\
    ((IORequest*)__req)->done(((IORequest*)__req)->context, (IORequest*)__req);\
}

#endif /* IORequest_h */
