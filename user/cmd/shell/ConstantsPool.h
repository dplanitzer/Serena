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


extern ConstantsPool* _Nonnull ConstantsPool_Create(void);
extern void ConstantsPool_Destroy(ConstantsPool* _Nullable self);

// Returns a uniqued string value from the constant pool. The string is added to
// the pool if it doesn't already exist there.
extern void ConstantsPool_GetStringValue(ConstantsPool* _Nonnull self, const char* _Nonnull str, size_t len, Value* _Nonnull vp);

#endif  /* ConstantsPool_h */
