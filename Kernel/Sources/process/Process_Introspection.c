//
//  Process_Introspection.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/10/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"


errno_t Process_Publish(ProcessRef _Locked _Nonnull self)
{
    if (self->catalogId == kCatalogId_None) {
        char buf[12];

        UInt32_ToString(self->pid, 10, false, buf);
        return Catalog_PublishProcess(gProcCatalog, buf, kUserId_Root, kGroupId_Root, FilePermissions_MakeFromOctal(0444), self, &self->catalogId);
    }
    else {
        return EOK;
    }
}

errno_t Process_Unpublish(ProcessRef _Locked _Nonnull self)
{
    if (self->catalogId != kCatalogId_None) {
        Catalog_Unpublish(gProcCatalog, kCatalogId_None, self->catalogId);
        self->catalogId = kCatalogId_None;
    }
    return EOK;
}
