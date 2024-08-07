//
//  ConstantsPool.h
//  sh
//
//  Created by Dietmar Planitzer on 7/22/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef ConstantsPool_h
#define ConstantsPool_h

#include "Errors.h"
#include "Value.h"


typedef struct Constant {
    struct Constant* _Nullable  next;       // Next constant in hash chain
    Value                       value;
} Constant;


typedef struct ConstantsPool {
    Constant **     hashtable;
    size_t          hashtableCapacity;
} ConstantsPool;


extern errno_t ConstantsPool_Create(ConstantsPool* _Nullable * _Nonnull pOutSelf);
extern void ConstantsPool_Destroy(ConstantsPool* _Nullable self);

// Returns a uniqued string value from the constant pool. The string is added to
// the pool if it doesn't already exist there.
extern errno_t ConstantsPool_GetStringValue(ConstantsPool* _Nonnull self, const char* _Nonnull str, size_t len, Value* _Nonnull pOutValue);

#endif  /* ConstantsPool_h */
