//
//  File.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_FILE_H
#define _SYS_FILE_H 1

#include <System/_cmndef.h>
#include <System/_syslimits.h>
#include <System/Error.h>
#include <System/FilePermissions.h>
#include <stdint.h>
#include <time.h>
#ifdef __KERNEL__
#include <kern/types.h>
#else
#include <sys/types.h>
#endif

__CPP_BEGIN

#define PATH_MAX __PATH_MAX
#define NAME_MAX __PATH_COMPONENT_MAX


// The Inode type.
#define S_IFREG     0   /* A regular file that stores data */
#define S_IFDIR     1   /* A directory which stores information about child nodes */
#define S_IFDEV     2   /* A driver which manages a piece of hardware */
#define S_IFFS      3   /* A mounted filesystem instance */
#define S_IFPROC    4   /* A process */
#define S_IFLNK     5
#define S_IFIFO     6

typedef uint16_t    FilePermissions;
typedef int8_t      FileType;


typedef struct finfo {
    struct timespec     accessTime;
    struct timespec     modificationTime;
    struct timespec     statusChangeTime;
    off_t               size;
    uid_t               uid;
    gid_t               gid;
    FilePermissions     permissions;
    FileType            type;
    char                reserved;
    nlink_t             linkCount;
    fsid_t              fsid;
    ino_t               inid;
} finfo_t;


enum ModifyFileInfo {
    kModifyFileInfo_AccessTime = 1,
    kModifyFileInfo_ModificationTime = 2,
    kModifyFileInfo_UserId = 4,
    kModifyFileInfo_GroupId = 8,
    kModifyFileInfo_Permissions = 16,
    kModifyFileInfo_All = kModifyFileInfo_AccessTime | kModifyFileInfo_ModificationTime
                        | kModifyFileInfo_UserId | kModifyFileInfo_GroupId
                        | kModifyFileInfo_Permissions
};

typedef struct fmutinfo {
    uint32_t            modify;
    struct timespec     accessTime;
    struct timespec     modificationTime;
    uid_t               uid;
    gid_t               gid;
    FilePermissions     permissions;
    uint16_t            permissionsModifyMask;  // Only modify permissions whose bit is set here
} fmutinfo_t;




// Returns meta-information about the file located at the filesystem location 'path'.
// @Concurrency: Safe
extern errno_t getfinfo(const char* _Nonnull path, finfo_t* _Nonnull info);

// Updates the meta-information about the file located at the filesystem location
// 'path'. Note that only those pieces of the meta-information are modified for
// which the corresponding flag in 'info.modify' is set.
// @Concurrency: Safe
extern errno_t setfinfo(const char* _Nonnull path, fmutinfo_t* _Nonnull info);



// Similar to getfinfo() but operates on the open file identified by 'ioc'.
// @Concurrency: Safe
extern errno_t fgetfinfo(int ioc, finfo_t* _Nonnull info);

// Similar to setfinfo() but operates on the open file identified by 'ioc'.
// @Concurrency: Safe
extern errno_t fsetfinfo(int ioc, fmutinfo_t* _Nonnull info);

__CPP_END

#endif /* _SYS_FILE_H */
