//
//  PathResolver.h
//  Apollo
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef PathResolver_h
#define PathResolver_h

#include "Filesystem.h"

#define kMaxPathLength          __PATH_MAX
#define kMaxPathComponentLength __PATH_COMPONENT_MAX


typedef struct _PathResolver {
    InodeRef        rootDirectory;
    InodeRef        currentWorkingDirectory;
    PathComponent   pathComponent;
    Character       nameBuffer[kMaxPathComponentLength + 1];
} PathResolver;
typedef struct _PathResolver* PathResolverRef;


// The path resolution mode
typedef enum _PathResolutionMode {
    // Return just the inode named by the path. This is the target node of the
    // path. An error and NULL is returned if no such node exists or if the node
    // is not accessible.
    kPathResolutionMode_TargetOnly,

    // Returns the inode named by the path if it exists and the parent inode if
    // the target inode does not exist but the parent inode does exist. Returns
    // an error and NULL if the target inode is not accessible or the resolution
    // fails for some other kind of error.
    kPathResolutionMode_TargetOrParent,

    // Similar to the TargetOrParent mode except that the lookup returns the
    // ancestor node that is closest to the target node and that does exist.
    // Otherwise behaves exactly like TargetOrParent mode.
    kPathResolutionMode_TargetOrAncestor

} PathResolutionMode;

// The result of a path resolution operation.
typedef struct _PathResolverResult {
    InodeRef _Nullable          inode;          // The inode named by the path if it exists and the parent inode otherwise, if requested
    FilesystemRef _Nullable     fileSystem;     // The filesystem that owns the returned inode

    const Character* _Nonnull   pathSuffix;     // Points to the first character of the path component following the parent or ancestor
} PathResolverResult;


// Must be called when you no longer need the path resolver result
extern void PathResolverResult_Deinit(PathResolverResult* pResult);


extern ErrorCode PathResolver_Init(PathResolverRef _Nonnull pResolver, InodeRef _Nonnull pRootDirectory, InodeRef _Nonnull pCurrentWorkingDirectory);
extern void PathResolver_Deinit(PathResolverRef _Nonnull pResolver);

extern ErrorCode PathResolver_SetRootDirectoryPath(PathResolverRef _Nonnull pResolver, User user, const Character* pPath);

extern ErrorCode PathResolver_GetCurrentWorkingDirectoryPath(PathResolverRef _Nonnull pResolver, User user, Character* pBuffer, ByteCount bufferSize);
extern ErrorCode PathResolver_SetCurrentWorkingDirectoryPath(PathResolverRef _Nonnull pResolver, User user, const Character* _Nonnull pPath);

extern ErrorCode PathResolver_CopyNodeForPath(PathResolverRef _Nonnull pResolver, PathResolutionMode mode, const Character* _Nonnull pPath, User user, PathResolverResult* _Nonnull pResult);

#endif /* PathResolver_h */
