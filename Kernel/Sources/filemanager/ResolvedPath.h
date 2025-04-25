//
//  ResolvedPath.h
//  kernel
//
//  Created by Dietmar Planitzer on 11/28/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef ResolvedPath_h
#define ResolvedPath_h

#include <filesystem/Inode.h>
#include <filesystem/PathComponent.h>


// The result of a path resolution operation.
typedef struct ResolvedPath {
    InodeRef _Nullable  inode;              // The target or the directory of the target node
    PathComponent       lastPathComponent;  // Last path component if the resolution mode is ParentOnly. Note that this stores a reference into the path that was passed to the resolution function
} ResolvedPath;


// Must be called once you no longer need the path resolver result.
extern void ResolvedPath_Deinit(ResolvedPath* self);

#endif /* ResolvedPath_h */
