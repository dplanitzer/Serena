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
#include <System/IOChannel.h>
#include <System/TimeInterval.h>
#include <System/Types.h>

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


typedef struct finfo {
    TimeInterval        accessTime;
    TimeInterval        modificationTime;
    TimeInterval        statusChangeTime;
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
    TimeInterval        accessTime;
    TimeInterval        modificationTime;
    uid_t               uid;
    gid_t               gid;
    FilePermissions     permissions;
    uint16_t            permissionsModifyMask;  // Only modify permissions whose bit is set here
} fmutinfo_t;


#define R_OK    1
#define W_OK    2
#define X_OK    4
#define F_OK    0


#define O_RDONLY    0x0001
#define O_WRONLY    0x0002
#define O_RDWR      (O_RDONLY | O_WRONLY)
#define O_APPEND    0x0004
#define O_EXCL      0x0008
#define O_TRUNC     0x0010
#define O_NONBLOCK  0x0020



// Creates an empty file at the filesystem location and with the name specified
// by 'path'. Creating a file is non-exclusive by default which means that the
// file is created if it does not exist and simply opened in it current state if
// it does exist. You may request non-exclusive behavior by passing the
// O_EXCL option. If the file already exists and you requested exclusive
// behavior, then this function will fail and return an EEXIST error. You may
// request that the newly opened file (relevant in non-exclusive mode) is
// automatically and atomically truncated to length 0 if it contained some data
// by passing the O_TRUNC option. 'permissions' are the file permissions
// that are assigned to a newly created file if it is actually created.
// @Concurrency: Safe
extern errno_t mkfile(const char* _Nonnull path, unsigned int mode, FilePermissions permissions, int* _Nonnull ioc);

// Opens an already existing file located at the filesystem location 'path'.
// Returns an error if the file does not exist or the caller lacks the necessary
// permissions to successfully open the file. 'mode' specifies whether the file
// should be opened for reading and/or writing. 'O_APPEND' may be passed in
// addition to 'O_WRONLY' to force the system to always append any newly written
// data to the file. The file position is disregarded by the write function(s) in
// this case.
// @Concurrency: Safe
extern errno_t open(const char* _Nonnull path, unsigned int mode, int* _Nonnull ioc);


// Truncates the file at the filesystem location 'path'. If the new length is
// greater than the size of the existing file, then the file is expanded and the
// newly added data range is zero-filled. If the new length is less than the
// size of the existing file, then the excess data is removed and the size of
// the file is set to the new length.
// @Concurrency: Safe
extern errno_t os_truncate(const char* _Nonnull path, off_t length);


// Returns meta-information about the file located at the filesystem location 'path'.
// @Concurrency: Safe
extern errno_t getfinfo(const char* _Nonnull path, finfo_t* _Nonnull info);

// Updates the meta-information about the file located at the filesystem location
// 'path'. Note that only those pieces of the meta-information are modified for
// which the corresponding flag in 'info.modify' is set.
// @Concurrency: Safe
extern errno_t setfinfo(const char* _Nonnull path, fmutinfo_t* _Nonnull info);


// Deletes the file or (empty) directory located at the filesystem location 'path'.
// Note that this function deletes empty directories only.
// @Concurrency: Safe
extern errno_t unlink(const char* _Nonnull path);

// Renames a file or directory located at the filesystem location 'oldpath' to
// the new name and filesystem location at 'newpath'. Both the old and the new
// filesystem location must reside in the same filesystem instance.
// @Concurrency: Safe
extern errno_t os_rename(const char* _Nonnull oldpath, const char* _Nonnull newpath);


// Similar to os_truncate() but operates on the open file identified by 'ioc'.
// @Concurrency: Safe
extern errno_t ftruncate(int ioc, off_t length);


// Similar to getfinfo() but operates on the open file identified by 'ioc'.
// @Concurrency: Safe
extern errno_t fgetfinfo(int ioc, finfo_t* _Nonnull info);

// Similar to setfinfo() but operates on the open file identified by 'ioc'.
// @Concurrency: Safe
extern errno_t fsetfinfo(int ioc, fmutinfo_t* _Nonnull info);

__CPP_END

#endif /* _SYS_FILE_H */
