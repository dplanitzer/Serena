//
//  ConstantPool.h
//  sh
//
//  Created by Dietmar Planitzer on 7/22/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef ConstantPool_h
#define ConstantPool_h

#include "Errors.h"
#include "Value.h"


typedef struct Constant {
    struct Constant* _Nullable  next;       // Next constant in hash chain
    Value                       value;
} Constant;


typedef struct ConstantPool {
    Constant **     hashtable;
    size_t          hashtableCapacity;
} ConstantPool;


extern errno_t ConstantPool_Create(ConstantPool* _Nullable * _Nonnull pOutSelf);
extern void ConstantPool_Destroy(ConstantPool* _Nullable self);

// Returns a uniqued string value from the constant pool. The string is added to
// the pool if it doesn't already exist there.
extern errno_t ConstantPool_GetStringValue(ConstantPool* _Nonnull self, const char* _Nonnull str, size_t len, Value* _Nonnull pOutValue);

#endif  /* ConstantPool_h */
