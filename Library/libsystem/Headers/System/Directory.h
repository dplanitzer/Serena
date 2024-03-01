//
//  Directory.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_DIRECTORY_H
#define _SYS_DIRECTORY_H 1

#include <System/_cmndef.h>
#include <System/_syslimits.h>
#include <System/Error.h>
#include <System/FilePermissions.h>
#include <System/Types.h>

__CPP_BEGIN

typedef struct DirectoryEntry {
    InodeId     inodeId;
    char        name[__PATH_COMPONENT_MAX];
} DirectoryEntry;


#if !defined(__KERNEL__)

// Creates an empty directory with the name and at the filesystem location specified
// by 'path'. 'mode' specifies the permissions that should be assigned to the
// directory.
// @Concurrency: Safe
extern errno_t Directory_Create(const char* _Nonnull path, FilePermissions mode);

// Opens the directory at the filesystem location 'path' for reading. Call this
// function to obtain an I/O channel suitable for reading the content of the
// directory. Call IOChannel_Close() once you are done with the directory.
// @Concurrency: Safe
extern errno_t Directory_Open(const char* _Nonnull path, int* _Nonnull ioc);

// Reads one or more directory entries from the directory identified by 'ioc'.
// Returns the number of directory entries actually read and returns 0 once all
// directory entries have been read.
// You can get the current directory entry position by calling File_GetPosition()
// and you can reestablish a previously saved directory entry position by calling
// File_Seek() with the result of a previous File_GetPosition() call.
// @Concurrency: Safe
extern errno_t Directory_Read(int ioc, DirectoryEntry* _Nonnull entries, size_t nEntriesToRead, ssize_t* _Nonnull nReadEntries);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_DIRECTORY_H */
