//
//  serena/filesystem.h
//  libc
//
//  Created by Dietmar Planitzer on 12/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SERENA_FILESYSTEM_H
#define _SERENA_FILESYSTEM_H 1

#include <_cmndef.h>
#include <kpi/filesystem.h>


// Returns information about the filesystem with id 'fsid'. 'flavor' selects the
// kinds of information that should be returned.
extern int fs_info(fsid_t fsid, int flavor, fs_info_ref _Nonnull info);

// Returns the path to the disk driver that underpins the filesystem with the
// given id.
extern int fs_diskpath(fsid_t fsid, char* _Nonnull buf, size_t bufSize);


// Returns the disk label of the filesystem with the given id.
extern int fs_label(fsid_t fsid, char* _Nonnull buf, size_t bufSize);

// Replaces the disk label of the filesystem with id 'fsid' with the new label
// 'label'.
extern int fs_setlabel(fsid_t fsid, const char* _Nonnull label);


// Mounts the object 'objectName' of type 'objectType' at the directory 'atDirPath'.
// 'params' are optional mount parameters that are passed to the filesystem to
// mount.
extern int fs_mount(const char* _Nonnull objectType, const char* _Nonnull objectName, const char* _Nonnull atDirPath, const char* _Nonnull params);

// Unmounts the filesystem mounted at the directory 'atDirPath'.
extern int fs_unmount(const char* _Nonnull atDirPath, int flags);


// Synchronously writes all dirty disk blocks back to disk.
extern void fs_sync(void);

#endif /* _SERENA_FILESYSTEM_H */
