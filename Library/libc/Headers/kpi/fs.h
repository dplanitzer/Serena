//
//  kpi/fs.h
//  libc
//
//  Created by Dietmar Planitzer on 12/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_FS_H
#define _KERN_FS_H 1

#include <kpi/types.h>
#include <kpi/ioctl.h>

// Filesystem properties
enum {
    kFSProperty_IsCatalog = 0x0001,     // Filesystem is a kernel managed catalog
    kFSProperty_IsRemovable = 0x0002,   // Filesystem lives on a removable/ejectable media
    kFSProperty_IsReadOnly = 0x0004,    // Filesystem was mounted read-only
};


// Filesystem specific information
typedef struct fsinfo {
    blkcnt_t        capacity;       // Filesystem capacity in terms of filesystem blocks (if a regular fs) or catalog entries (if a catalog)
    blkcnt_t        count;          // Blocks or entries currently in use/allocated
    size_t          blockSize;      // Size of a block in bytes
    fsid_t          fsid;           // Filesystem ID
    unsigned int    properties;     // Filesystem properties
    char            type[12];       // Filesystem type (max 11 characters C string)
} fsinfo_t;


//
// FS API
//

// Returns general information about the filesystem.
// get_fsinfo(fsinfo_t* _Nonnull pOutInfo)
#define kFSCommand_GetInfo          IOResourceCommand(0)

// Returns the label of a filesystem. The label is a name that can be assigned
// when a disk is formatted and that helps a user in identifying a disk. Note
// that not all filesystems support a label. ENOTSUP is returned in this case.
// get_label(char* _Nonnull buf, size_t bufSize)
#define kFSCommand_GetLabel         IOResourceCommand(1)

// Sets the label of a filesystem. Note that not all filesystems support a label.
// ENOTSUP is returned in this case.
// set_label(const char* _Nonnull buf)
#define kFSCommand_SetLabel         IOResourceCommand(2)

// Returns information about the disk that holds the filesystem.
// ENOMEDIUM is returned if no disk is in the drive. Returns ENOTSUP if the
// filesystem isn't disk-based.
// get_geometry(disk_info_t* _Nonnull pOutInfo)
#define kFSCommand_GetDiskInfo      IOResourceCommand(3)


// Instruct the filesystem to flush all cached meta and other data to the disk.
// Blocks the caller until all data has been synced to disk. Only data belonging
// to the filesystem to synced. Data belonging to other filesystems remains in
// the cache and is not touched.
// fssync(void)
#define kFSCommand_Sync             IOResourceCommand(4)

#endif /* _KERN_FS_H */
