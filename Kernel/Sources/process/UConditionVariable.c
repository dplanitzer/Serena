//
//  UConditionVariable.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "UConditionVariable.h"


errno_t UConditionVariable_Create(UConditionVariableRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    UConditionVariableRef self;

    try(Object_Create(UConditionVariable, &self));
    ConditionVariable_Init(&self->cv);
    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    return err;
}

void UConditionVariable_deinit(UConditionVariableRef _Nonnull self)
{
    ConditionVariable_Deinit(&self->cv);
}

CLASS_METHODS(UConditionVariable, Object,
OVERRIDE_METHOD_IMPL(deinit, UConditionVariable, Object)
);
