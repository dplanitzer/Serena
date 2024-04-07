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
    FilesystemRef _Nonnull  rooFilesystem;
    InodeRef _Nonnull       rootDirectory;

    FilesystemRef _Nonnull  workingFilesystem;
    InodeRef _Nonnull       workingDirectory;

    PathComponent           pathComponent;
    char                    nameBuffer[kMaxPathComponentLength + 1];
} PathResolver;
typedef PathResolver* PathResolverRef;


// The path resolution mode
typedef enum PathResolutionMode {
    // Return the inode named by the path. This is the target node of the path.
    // An error and NULL is returned if no such node exists or if the node is
    // not accessible.
    kPathResolutionMode_TargetOnly,

    // Returns the inode that is the parent of the inode named by the path. An
    // error and NULL is returned if no such node exists or is accessible.
    kPathResolutionMode_ParentOnly

} PathResolutionMode;

// The result of a path resolution operation.
typedef struct PathResolverResult {
    InodeRef _Nullable _Locked  inode;              // The inode named by the path if it exists and the parent inode otherwise, if requested
    FilesystemRef _Nullable     filesystem;         // The filesystem that owns the returned inode

    PathComponent               lastPathComponent;  // Last path component if the resolution mode is ParentOnly. Note that this stores a reference into the path that was passed to the resolution function
} PathResolverResult;


// Must be called once you no longer need the path resolver result.
extern void PathResolverResult_Deinit(PathResolverResult* self);


extern errno_t PathResolver_Create(FilesystemRef _Nonnull pRootFs, PathResolverRef _Nullable * _Nonnull pOutSelf);
extern errno_t PathResolver_CreateCopy(PathResolverRef _Nonnull pOther, PathResolverRef _Nullable * _Nonnull pOutSelf);
extern void PathResolver_Destroy(PathResolverRef _Nullable self);

extern errno_t PathResolver_SetRootDirectoryPath(PathResolverRef _Nonnull self, User user, const char* _Nonnull pPath);
extern bool PathResolver_IsRootDirectory(PathResolverRef _Nonnull self, InodeRef _Nonnull _Locked pNode);

extern errno_t PathResolver_GetWorkingDirectoryPath(PathResolverRef _Nonnull self, User user, char* _Nonnull pBuffer, size_t bufferSize);
extern errno_t PathResolver_SetWorkingDirectoryPath(PathResolverRef _Nonnull self, User user, const char* _Nonnull pPath);

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
extern errno_t PathResolver_AcquireNodeForPath(PathResolverRef _Nonnull self, PathResolutionMode mode, const char* _Nonnull pPath, User user, PathResolverResult* _Nonnull pResult);

#endif /* PathResolver_h */
