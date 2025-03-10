//
//  DfsNode.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef DfsNode_h
#define DfsNode_h

#include <filesystem/Inode.h>
#include <klib/List.h>

    
open_class(DfsNode, Inode,
    ListNode    inChain;
);
open_class_funcs(DfsNode, Inode,
);

#endif /* DfsNode_h */
