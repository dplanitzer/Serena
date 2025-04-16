//
//  Filesystem.h
//  libsystem
//
//  Created by Dietmar Planitzer on 12/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_FILESYSTEM_H
#define _SYS_FILESYSTEM_H 1

#include <System/_cmndef.h>
#include <System/Error.h>
#include <System/Types.h>
#include <System/IOChannel.h>

__CPP_BEGIN

enum {
    kMount_Disk,            // 'objectName' is the path to a disk device
    kMount_Catalog,         // 'objectName' is a catalog name
};
typedef int MountType;


// Mountable catalogs
#define kCatalogName_Drivers        "dev"
#define kCatalogName_Filesystems    "fs"


enum {
    kUnmount_Forced = 0x0001,   // Force the unmount even if there are still files open
};
typedef unsigned int UnmountOptions;


// Filesystem properties
enum {
    kFSProperty_IsCatalog = 0x0001,     // Filesystem is a kernel managed catalog
    kFSProperty_IsReadOnly = 0x0002,    // Filesystem was mounted read-only
    kFSProperty_IsEjectable = 0x0004,   // Filesystem lives on an ejectable media
};

// Filesystem specific information
typedef struct FSInfo {
    LogicalBlockCount   capacity;       // Filesystem capacity in terms of filesystem blocks (if a regular fs) or catalog entries (if a catalog)
    LogicalBlockCount   count;          // Blocks or entries currently in use/allocated
    size_t              blockSize;      // Size of a block in bytes
    fsid_t              fsid;           // Filesystem ID
    MediaId             mediaId;        // Media on which the filesystem lives
    uint32_t            properties;     // Filesystem properties
    char                type[12];       // Filesystem type (max 11 characters C string)
} FSInfo;


//
// FS API
//

// Returns general information about the filesystem.
// get_fsinfo(FSInfo* _Nonnull pOutInfo)
#define kFSCommand_GetInfo      IOResourceCommand(0)

// Returns the canonical name of the disk on which the filesystem resides. Note
// that not all filesystems are disk-based. Eg kernel object catalogs like /dev
// or /fs are not disk-based and thus they return an empty string as the disk
// name. 
// get_get_disk_name(size_t bufSize, char* _Nonnull buf)
#define kFSCommand_GetDiskName  IOResourceCommand(1)

// Returns the label of a filesystem. The label is a name that can be assigned
// when a disk is formatted and that helps a user in identifying a disk. Note
// that not all filesystems support a label. ENOTSUP is returned in this case.
// get_label(size_t bufSize, char* _Nonnull buf)
#define kFSCommand_GetLabel     IOResourceCommand(2)

// Sets the label of a filesystem. Note that not all filesystems support a label.
// ENOTSUP is returned in this case.
// set_label(const char* _Nonnull buf)
#define kFSCommand_SetLabel     IOResourceCommand(3)


#if !defined(__KERNEL__)

// Mounts the object 'objectName' of type 'type' at the directory 'atDirPath'.
// 'params' are optional mount parameters that are passed to the filesystem to
// mount.
extern errno_t Mount(MountType type, const char* _Nonnull objectName, const char* _Nonnull atDirPath, const void* _Nullable params, size_t paramsSize);

// Unmounts the filesystem mounted at the directory 'atDirPath'.
extern errno_t Unmount(const char* _Nonnull atDirPath, UnmountOptions options);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_FILESYSTEM_H */
