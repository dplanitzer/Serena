//
//  FileHierarchy.h
//  kernel
//
//  Created by Dietmar Planitzer on 11/28/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef FileHierarchy_h
#define FileHierarchy_h

#include "Filesystem.h"

#define kMaxPathLength          (__PATH_MAX-1)
#define kMaxPathComponentLength __PATH_COMPONENT_MAX


// Represents and manages the file(system) hierarchy for a process. A process
// inherits its file hierarchy by default from its parent process.
// This class guarantees that the file hierarchy does not change while a path
// resolution is in progress.
final_class(FileHierarchy, Object);


// The path resolution mode
typedef enum PathResolution {
    // Returns the inode named by the path. This is the target node of the path.
    // An error and NULL is returned if no such node exists or if the node is
    // not accessible.
    kPathResolution_Target,

    // Returns the predecessor directory of the target and the last path
    // component of the path. The predecessor directory is the directory named
    // by the path component that comes immediately before the target path
    // component. NULL and a suitable error is returned if the predecessor of
    // the target can not be resolved.
    kPathResolution_PredecessorOfTarget
} PathResolution;


// The result of a path resolution operation.
typedef struct ResolvedPath {
    InodeRef _Nullable  inode;              // The target or the directory of the target node
    PathComponent       lastPathComponent;  // Last path component if the resolution mode is ParentOnly. Note that this stores a reference into the path that was passed to the resolution function
} ResolvedPath;


// Must be called once you no longer need the path resolver result.
extern void ResolvedPath_Deinit(ResolvedPath* self);


extern errno_t FileHierarchy_Create(FilesystemRef _Nonnull rootFS, FileHierarchyRef _Nullable * _Nonnull pOutSelf);

// Returns a strong reference to the root filesystem of the given file hierarchy.
extern FilesystemRef FileHierarchy_CopyRootFilesystem(FileHierarchyRef _Nonnull self);

// Returns the root directory of the given file hierarchy.
extern InodeRef _Nonnull FileHierarchy_AcquireRootDirectory(FileHierarchyRef _Nonnull self);


// Attaches the root directory of the filesystem 'fs' to the directory 'atDir'. 'atDir'
// must be a member of this file hierarchy and may not already have another filesystem
// attached to it.
extern errno_t FileHierarchy_AttachFilesystem(FileHierarchyRef _Nonnull self, FilesystemRef _Nonnull fs, InodeRef _Nonnull atDir);

// Detaches the filesystem attached to directory 'dir'
extern errno_t FileHierarchy_DetachFilesystemAt(FileHierarchyRef _Nonnull self, InodeRef _Nonnull dir);

// Returns true if the given (directory) inode is an attachment point for another
// filesystem.
extern bool FileHierarchy_IsAttachmentPoint(FileHierarchyRef _Nonnull self, InodeRef _Nonnull inode);


// Returns a path from 'rootDir' to 'dir' in 'buffer'.
extern errno_t FileHierarchy_GetDirectoryPath(FileHierarchyRef _Nonnull self, InodeRef _Nonnull dir, InodeRef _Nonnull rootDir, User user, char* _Nonnull  pBuffer, size_t bufferSize);

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
extern errno_t FileHierarchy_AcquireNodeForPath(FileHierarchyRef _Nonnull self, PathResolution mode, const char* _Nonnull path, InodeRef _Nonnull rootDir, InodeRef _Nonnull cwDir, User user, ResolvedPath* _Nonnull pResult);

#endif /* FileHierarchy_h */
