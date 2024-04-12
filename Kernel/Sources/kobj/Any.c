//
//  Any.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Any.h"


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
