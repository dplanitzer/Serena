//
//  PathResolver.h
//  kernel
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef PathResolver_h
#define PathResolver_h

#include "Filesystem.h"

#define kMaxPathLength          (__PATH_MAX-1)
#define kMaxPathComponentLength __PATH_COMPONENT_MAX


typedef struct PathResolver {
    InodeRef _Nonnull   rootDirectory;
    InodeRef _Nonnull   workingDirectory;
    User                user;
} PathResolver;
typedef PathResolver* PathResolverRef;


// The path resolution mode
typedef enum PathResolverMode {
    // Returns the inode named by the path. This is the target node of the path.
    // An error and NULL is returned if no such node exists or if the node is
    // not accessible.
    kPathResolverMode_Target,

    // Returns the predecessor directory of the target and the last path
    // component of the path. The predecessor directory is the directory named
    // by the path component that comes immediately before the target path
    // component. NULL and a suitable error is returned if the predecessor of
    // the target can not be resolved.
    kPathResolverMode_PredecessorOfTarget
} PathResolverMode;


// The result of a path resolution operation.
typedef struct PathResolverResult {
    InodeRef _Nullable  inode;              // The target or the directory of the target node
    PathComponent       lastPathComponent;  // Last path component if the resolution mode is ParentOnly. Note that this stores a reference into the path that was passed to the resolution function
} PathResolverResult;


// Must be called once you no longer need the path resolver result.
extern void PathResolverResult_Deinit(PathResolverResult* self);


extern void PathResolver_Init(PathResolverRef _Nonnull self, InodeRef _Nonnull pRootDir, InodeRef _Nonnull pWorkingDir, User user);
extern void PathResolver_Deinit(PathResolverRef _Nullable self);

extern errno_t PathResolver_GetDirectoryPath(PathResolverRef _Nonnull self, InodeRef _Nonnull pStartDir, char* _Nonnull  pBuffer, size_t bufferSize);

// Looks up the inode named by the given path. The path may be relative or absolute.
// If it is relative then the resolution starts with the current working directory.
// If it is absolute then the resolution starts with the root directory. The path
// may contain the well-known name '.' which stands for 'this directory' and '..'
// which stands for 'the parent directory'. Note that this function does not allow
// you to leave the subtree rooted by the root directory. Any attempt to go to a
// parent of the root directory will send you back to the root directory.
// The caller of this function has to call PathResolverResult_Deinit() on the
// returned result when no longer needed, no matter whether this function has
// returned with EOK or some error.
extern errno_t PathResolver_AcquireNodeForPath(PathResolverRef _Nonnull self, PathResolverMode mode, const char* _Nonnull pPath, PathResolverResult* _Nonnull pResult);

// Looks up the node that corresponds to the path component 'pComponent' which is
// assumed to be relative to the directory 'pDir'. The path component may be a
// file/directory name, '..' or '.'.
extern errno_t PathResolver_AcquireNodeForPathComponent(PathResolverRef _Nonnull self, InodeRef _Nonnull pDir, const PathComponent* _Nonnull pComponent, InodeRef _Nullable * _Nonnull pOutNode);

#endif /* PathResolver_h */
