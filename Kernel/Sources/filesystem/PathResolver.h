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
    InodeRef _Nonnull       rootDirectory;
    InodeRef _Nonnull       workingDirectory;
} PathResolver;
typedef PathResolver* PathResolverRef;


// The path resolution mode
typedef enum PathResolverMode {
    // Returns the inode named by the path. This is the target node of the path.
    // An error and NULL is returned if no such node exists or if the node is
    // not accessible.
    kPathResolverMode_TargetOnly,

    // Returns the target node if it exists or the immediate parent node and the
    // last path component if the target does not exist.
    kPathResolverMode_TargetOrParent,

    // Returns the target and the parent node. Both nodes are expected to exist.
    // Returns a suitable error if neither exists.
    kPathResolverMode_TargetAndParent,

    // Returns the parent directory of the target and the last path component.
    // Additionally returns the target node if it exists. The parent node is
    // expected to exist in any case. Returns a suitable error if the parent
    // does not exist.
    kPathResolverMode_OptionalTargetAndParent,

} PathResolverMode;


// Path resolution options
enum PathResolverOption {
    kPathResolverOption_NotYet = 1
};
typedef uint32_t PathResolverOptions;


// The result of a path resolution operation.
typedef struct PathResolverResult {
    InodeRef _Nullable          parent;             // Parent node if requested
    InodeRef _Nullable          target;             // Target node if requested
    PathComponent               lastPathComponent;  // Last path component if the resolution mode is ParentOnly. Note that this stores a reference into the path that was passed to the resolution function
    DirectoryEntryInsertionHint insertionHint;      // Insertion hint for inserting the last path component into 'parent' if the resolution mode is TargetOrParent and the target doesn't exist; otherwise undefined
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
extern errno_t PathResolver_AcquireNodeForPath(PathResolverRef _Nonnull self, PathResolverMode mode, const char* _Nonnull pPath, User user, PathResolverResult* _Nonnull pResult);

#endif /* PathResolver_h */
