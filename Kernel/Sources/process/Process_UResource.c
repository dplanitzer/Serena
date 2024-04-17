//
//  Process_UResource.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"


// Disposes the user resource identified by the given descriptor. The resource
// is deallocated and removed from the resource table.
errno_t Process_DisposeUResource(ProcessRef _Nonnull pProc, int od)
{
    return UResourceTable_DisposeResource(&pProc->uResourcesTable, od);
}
