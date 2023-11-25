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


// The result of a path resolution operation.
typedef struct _PathResolverResult {
    InodeRef _Nullable      ancestorNode;       // Existing and accessible ancestor node closest to the targetNode
    FilesystemRef _Nullable ancestorFilesystem; // The filesystem of the ancestor node

    InodeRef _Nullable      targetNode;         // Node named by the path, if it exists
    FilesystemRef _Nullable targetFilesystem;   // Filesystem owned the node named by the path, if it exists
    ErrorCode               error;              // EOK if the target node exists and is accessible; suitable error code otherwise
} PathResolverResult;


// Must be called when you no longer need the path resolver result
extern void PathResolverResult_Deinit(PathResolverResult* pResult);


extern ErrorCode PathResolver_Init(PathResolverRef _Nonnull pResolver, InodeRef _Nonnull pRootDirectory, InodeRef _Nonnull pCurrentWorkingDirectory);
extern void PathResolver_Deinit(PathResolverRef _Nonnull pResolver);

extern ErrorCode PathResolver_SetRootDirectoryPath(PathResolverRef _Nonnull pResolver, User user, const Character* pPath);

extern ErrorCode PathResolver_GetCurrentWorkingDirectoryPath(PathResolverRef _Nonnull pResolver, User user, Character* pBuffer, ByteCount bufferSize);
extern ErrorCode PathResolver_SetCurrentWorkingDirectoryPath(PathResolverRef _Nonnull pResolver, User user, const Character* _Nonnull pPath);

enum {
    // Return information about the node and filesystem that is the closest existing
    // and accessible ancestor node of the path's target node. Usually that is the
    // immediate parent node (directory) of the target node. It may however be a
    // grandparent if the target node and neither it's parent exists or is accessible.
    kPathResolutionOption_IncludeAncestor = 1
};

extern PathResolverResult PathResolver_CopyNodeForPath(PathResolverRef _Nonnull pResolver, const Character* _Nonnull pPath, UInt options, User user);

#endif /* PathResolver_h */
