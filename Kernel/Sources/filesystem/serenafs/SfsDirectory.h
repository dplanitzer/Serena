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


typedef struct sfs_insertion_hint {
    LogicalBlockAddress lba;
    size_t              blockOffset;
} sfs_insertion_hint_t;


enum {
    kSFSQuery_PathComponent,
    kSFSQuery_InodeId
};


typedef struct sfs_query {
    int         kind;
    union _qu {
        const PathComponent* _Nonnull   pc;
        InodeId                         id;
    }           u;
    MutablePathComponent* _Nullable mpc;    // Query result will update this with a filename if the query is a kSFSQuery_InodeId query
    sfs_insertion_hint_t* _Nullable ih;     // Query result will update this with an insertion hint if this is != NULL
} sfs_query_t;


typedef struct sfs_query_result {    
    MutablePathComponent* _Nullable mpc;
    InodeId                         id;
    LogicalBlockAddress             lba;            // LBA of the disk block that holds the directory entry
    size_t                          blockOffset;    // Byte offset to the directory entry relative to the disk block start
    off_t                           fileOffset;     // Byte offset relative to the start of the directory file
    sfs_insertion_hint_t* _Nullable ih;
} sfs_query_result_t;


open_class(SfsDirectory, SfsFile,
);
open_class_funcs(SfsDirectory, SfsFile,
);


extern bool SfsDirectory_IsNotEmpty(InodeRef _Nonnull _Locked self);
extern errno_t SfsDirectory_Query(InodeRef _Nonnull _Locked self, sfs_query_t* _Nonnull q, sfs_query_result_t* _Nonnull qr);
extern errno_t SfsDirectory_RemoveEntry(InodeRef _Nonnull _Locked self, InodeId idToRemove);
extern errno_t SfsDirectory_InsertEntry(InodeRef _Nonnull _Locked self, const PathComponent* _Nonnull pName, InodeId id, const sfs_insertion_hint_t* _Nullable ih);

#endif /* SfsDirectory_h */
