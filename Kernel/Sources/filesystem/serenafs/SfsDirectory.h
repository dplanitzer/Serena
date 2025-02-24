//
//  SfsDirectory.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef SfsDirectory_h
#define SfsDirectory_h

#include "SfsFile.h"
#include <filesystem/PathComponent.h>


enum SFSDirectoryQueryKind {
    kSFSDirectoryQuery_PathComponent,
    kSFSDirectoryQuery_InodeId
};

typedef struct SFSDirectoryQuery {
    int     kind;
    union _u {
        const PathComponent* _Nonnull   pc;
        InodeId                         id;
    }       u;
} SFSDirectoryQuery;

// Points to a directory entry inside a disk block
typedef struct sfs_dirent_ptr {
    LogicalBlockAddress     lba;            // LBA of the disk block that holds the directory entry
    size_t                  blockOffset;    // Byte offset to the directory entry relative to the disk block start
    FileOffset              fileOffset;     // Byte offset relative to the start of the directory file
} sfs_dirent_ptr_t;


open_class(SfsDirectory, SfsFile,
);
open_class_funcs(SfsDirectory, SfsFile,
);


extern bool SfsDirectory_IsNotEmpty(InodeRef _Nonnull _Locked self);
extern errno_t SfsDirectory_GetEntry(InodeRef _Nonnull _Locked self, const SFSDirectoryQuery* _Nonnull pQuery, sfs_dirent_ptr_t* _Nullable pOutEmptyPtr, sfs_dirent_ptr_t* _Nullable pOutEntryPtr, InodeId* _Nullable pOutId, MutablePathComponent* _Nullable pOutFilename);
extern errno_t SfsDirectory_RemoveEntry(InodeRef _Nonnull _Locked self, InodeId idToRemove);
extern errno_t SfsDirectory_InsertEntry(InodeRef _Nonnull _Locked self, const PathComponent* _Nonnull pName, InodeId id, const sfs_dirent_ptr_t* _Nullable pEmptyPtr);

#endif /* SfsDirectory_h */
