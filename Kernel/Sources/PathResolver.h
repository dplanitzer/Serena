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

    // Returns just the inode that is the parent of the inode named by the path.
    // An error and NULL is returned if no such node exists or is accessible.
    kPathResolutionMode_ParentOnly

} PathResolutionMode;

// The result of a path resolution operation.
typedef struct _PathResolverResult {
    InodeRef _Nullable _Locked  inode;          // The inode named by the path if it exists and the parent inode otherwise, if requested
    FilesystemRef _Nullable     filesystem;     // The filesystem that owns the returned inode

    PathComponent               lastPathComponent;  // Last path component if the resolution mode is ParentOnly. Note that this stores a reference into the path that was passed to the resolution function
} PathResolverResult;


// Must be called when you no longer need the path resolver result
extern void PathResolverResult_Deinit(PathResolverResult* pResult);


extern ErrorCode PathResolver_Init(PathResolverRef _Nonnull pResolver, InodeRef _Nonnull pRootDirectory, InodeRef _Nonnull pCurrentWorkingDirectory);
extern void PathResolver_Deinit(PathResolverRef _Nonnull pResolver);

extern ErrorCode PathResolver_SetRootDirectoryPath(PathResolverRef _Nonnull pResolver, User user, const Character* pPath);

extern ErrorCode PathResolver_GetCurrentWorkingDirectoryPath(PathResolverRef _Nonnull pResolver, User user, Character* pBuffer, ByteCount bufferSize);
extern ErrorCode PathResolver_SetCurrentWorkingDirectoryPath(PathResolverRef _Nonnull pResolver, User user, const Character* _Nonnull pPath);

// Looks up the inode named by the given path. The path may be relative or absolute.
// If it is relative then the resolution starts with the current working directory.
// If it is absolute then the resolution starts with the root directory. The path
// may contain the well-known name '.' which stands for 'this directory' and '..'
// which stands for 'the parent directory'. Note that this function does not allow
// you to leave the subtree rotted by the root directory. Any attempt to go to a
// parent of the root directory will send you back to the root directory.
// Note that the caller of this function has to eventually call
// PathResolverResult_Deinit() on the returned result no matter whether this
// function has returned with EOK or some error.
extern ErrorCode PathResolver_AcquireNodeForPath(PathResolverRef _Nonnull pResolver, PathResolutionMode mode, const Character* _Nonnull pPath, User user, PathResolverResult* _Nonnull pResult);

#endif /* PathResolver_h */
