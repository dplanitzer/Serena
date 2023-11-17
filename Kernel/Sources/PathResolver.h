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


extern ErrorCode PathResolver_Init(PathResolverRef _Nonnull pResolver, InodeRef _Nonnull pRootDirectory, InodeRef _Nonnull pCurrentWorkingDirectory);
extern void PathResolver_Deinit(PathResolverRef _Nonnull pResolver);

extern ErrorCode PathResolver_SetRootDirectoryPath(PathResolverRef _Nonnull pResolver, const Character* pPath);

extern ErrorCode PathResolver_GetCurrentWorkingDirectoryPath(PathResolverRef _Nonnull pResolver, Character* pBuffer, ByteCount bufferSize);
extern ErrorCode PathResolver_SetCurrentWorkingDirectoryPath(PathResolverRef _Nonnull pResolver, const Character* _Nonnull pPath);

extern ErrorCode PathResolver_CopyNodeForPath(PathResolverRef _Nonnull pResolver, const Character* _Nonnull pPath, InodeRef _Nullable * _Nonnull pOutNode);


extern ErrorCode MakePathFromInode(InodeRef _Nonnull pNode, InodeRef _Nonnull pRootDir, Character* pBuffer, ByteCount bufferSize);

#endif /* PathResolver_h */
