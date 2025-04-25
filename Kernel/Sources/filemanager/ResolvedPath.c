//
//  ResolvedPath.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/28/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ResolvedPath.h"


void ResolvedPath_Init(ResolvedPath* _Nonnull self)
{
    self->inode = NULL;
    self->lastPathComponent.name = NULL;
    self->lastPathComponent.count = 0;
}

void ResolvedPath_Deinit(ResolvedPath* _Nonnull self)
{
    if (self->inode) {
        Inode_Relinquish(self->inode);
        self->inode = NULL;
    }
}
