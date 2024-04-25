//
//  Any.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Any.h"
#include <klib/Assert.h>

any_class_def(Any);

bool _instanceof(Any* _Nonnull pAny, Class* _Nonnull pTargetType)
{
    Class* curTargetType = pTargetType;
    Class* anyType = classof(pAny);

    while (curTargetType) {
        if (anyType == curTargetType) {
            return true;
        }
        curTargetType = curTargetType->super;
    }

    return false;
}

// Returns the class that defines the super implementation of the method identified
// by the method offset 'methodOffset'.
Class* _Nonnull _superimplclassof(Any* _Nonnull self, size_t methodOffset)
{
    Class* pPrevClass = classof(self);
    MethodImpl pPrevImpl = *(MethodImpl*)((char*)(pPrevClass->vtable) + methodOffset);

    for(;;) {
        Class* pCurClass = pPrevClass->super;
        if (pCurClass == NULL || pCurClass == pPrevClass) {
            // The top type can't call super for obvious reasons
            abort();
        }

        MethodImpl pCurImpl = *(MethodImpl*)((char*)(pCurClass->vtable) + methodOffset);
        if (pCurImpl != pPrevImpl) {
            return pCurClass;
        }

        pPrevClass = pCurClass;
        pPrevImpl = pCurImpl;
    }
}
