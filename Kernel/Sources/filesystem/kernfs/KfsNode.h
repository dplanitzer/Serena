//
//  KfsNode.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef KfsNode_h
#define KfsNode_h

#include <filesystem/Inode.h>
#include <klib/List.h>

    
open_class(KfsNode, Inode,
    ListNode    inChain;
);
open_class_funcs(KfsNode, Inode,
);

#endif /* KfsNode_h */
