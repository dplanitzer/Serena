//
//  kpi/stat.h
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_STAT_H
#define _KPI_STAT_H 1

#include <kpi/syslimits.h>
#include <kpi/_time.h>
#include <kpi/types.h>

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

typedef unsigned short  FilePermissions;
typedef char            FileType;


// File permissions. Every file and directory has 3 sets of permissions associated
// with it (also knows as "permission classes"):
// Owner class (of the file)
// Group class (the file is associated with)
// Other class (anyone else who isn't the owner)
//
// The meaning of the permission bits for files are:
// R    Allow reading of the file contents
// W    Allow writing/updating the file contents
// X    Allow executing the file. The file must contain data in the executable format
//
// The meaning of the permission bits for directories are:
// R    Allow reading the directory listing
// W    Allow adding/removing directory entries
// X    Allow searching the directory listing
//
// Note that a FilePermissions value type holds permission bits for all three
// permission classes.
enum {
    kFilePermission_Read = 0x04,
    kFilePermission_Write = 0x02,
    kFilePermission_Execute = 0x01
};

enum {
    kFilePermissionsClass_BitWidth = 3,

    kFilePermissionsClass_User = 2*kFilePermissionsClass_BitWidth,
    kFilePermissionsClass_Group = kFilePermissionsClass_BitWidth,
    kFilePermissionsClass_Other = 0,

    kFilePermissionsClass_Mask = 0x07,
};

// Creates a FilePermission value with permissions for a user, group and other
// permission class.
#define FilePermissions_Make(__user, __group, __other) \
  (((__user) & kFilePermissionsClass_Mask) << kFilePermissionsClass_User) \
| (((__group) & kFilePermissionsClass_Mask) << kFilePermissionsClass_Group) \
| (((__other) & kFilePermissionsClass_Mask) << kFilePermissionsClass_Other)

// Creates a FilePermission value from a POSIX style octal number. This number
// is expected to be a 3 digit number where each digit represents one of the
// permission classes.
#define FilePermissions_MakeFromOctal(__3_x_3_octal) \
    (__3_x_3_octal)
    
// Returns the permission bits of '__permission' that correspond to the
// permissions class '__class'.
#define FilePermissions_Get(__permissions, __class) \
(((__permissions) >> (__class)) & kFilePermissionsClass_Mask)

// Returns true if the permission '__permission' is set in the class '__class'
// of the permissions '__permission'.
#define FilePermissions_Has(__permissions, __class, __permission) \
((FilePermissions_Get(__permissions, __class) & (__permission)) == (__permission))

// Adds the permission bits '__bits' to the class '__class' in the file permission
// set '__permissions' 
#define FilePermissions_Add(__permissions, __class, __bits) \
 (__permissions) |= (((__bits) & kFilePermissionsClass_Mask) << (__class))

// Removes the permission bits '__bits' from the class '__class' in the file
// permission set '__permissions' 
#define FilePermissions_Remove(__permissions, __class, __bits) \
 (__permissions) &= ~(((__bits) & kFilePermissionsClass_Mask) << (__class))

// Replaces all permission bits in the class '__class' of the file permission
// set '__permissions' with the new permission bits '__bits' 
#define FilePermissions_Set(__permissions, __class, __bits) \
 (__permissions) = ((__permissions) & ~(kFilePermissionsClass_Mask << (__class))) | (((__bits) & kFilePermissionsClass_Mask) << (__class))



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
    unsigned int        modify;
    struct timespec     accessTime;
    struct timespec     modificationTime;
    uid_t               uid;
    gid_t               gid;
    FilePermissions     permissions;
    unsigned short      permissionsModifyMask;  // Only modify permissions whose bit is set here
} fmutinfo_t;


// Tells utimens() to set the file timestamp to the current time. Assign to the
// tv_nsec field.
#define UTIME_NOW   -1

// Tells utimens() to set the file timestamp to leave the file timestamp
// unchanged. Assign to the tv_nsec field.
#define UTIME_OMIT  -2


// Order of the utimesns() timestamp
#define UTIME_ACCESS        0
#define UTIME_MODIFICATION  1


// Tell umask() to just return the current umask without changing it
#define SEO_UMASK_NO_CHANGE -1

#endif /* _KPI_STAT_H */
