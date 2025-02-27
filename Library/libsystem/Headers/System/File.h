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
enum FileType {
    kFileType_RegularFile = 0,  // A regular file that stores data
    kFileType_Directory,        // A directory which stores information about child nodes
    kFileType_Device,           // A driver which manages a piece of hardware
    kFileType_SymbolicLink,
    kFileType_Pipe,
};


typedef struct FileInfo {
    TimeInterval        accessTime;
    TimeInterval        modificationTime;
    TimeInterval        statusChangeTime;
    off_t               size;
    UserId              uid;
    GroupId             gid;
    FilePermissions     permissions;
    FileType            type;
    char                reserved;
    int                 linkCount;
    FilesystemId        filesystemId;
    InodeId             inodeId;
} FileInfo;


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

typedef struct MutableFileInfo {
    uint32_t            modify;
    TimeInterval        accessTime;
    TimeInterval        modificationTime;
    UserId              uid;
    GroupId             gid;
    FilePermissions     permissions;
    uint16_t            permissionsModifyMask;  // Only modify permissions whose bit is set here
} MutableFileInfo;


enum AccessMode {
    kAccess_Readable = 1,
    kAccess_Writable = 2,
    kAccess_Executable = 4,
    kAccess_Searchable = kAccess_Executable,    // For directories
    kAccess_Exists = 0
};
typedef uint32_t    AccessMode;


#define kOpen_Read          0x0001
#define kOpen_Write         0x0002
#define kOpen_ReadWrite     (kOpen_Read | kOpen_Write)
#define kOpen_Append        0x0004
#define kOpen_Exclusive     0x0008
#define kOpen_Truncate      0x0010


// Specifies how a File_Seek() call should apply 'offset' to the current file
// position.
enum SeekMode {
    kSeek_Set = 0,      // Set the file position to 'offset'
    kSeek_Current,      // Add 'offset' to the current file position
    kSeek_End           // Add 'offset' to the end of the file
};


#if !defined(__KERNEL__)

// Creates an empty file at the filesystem location and with the name specified
// by 'path'. Creating a file is non-exclusive by default which means that the
// file is created if it does not exist and simply opened in it current state if
// it does exist. You may request non-exclusive behavior by passing the
// kOpen_Exclusive option. If the file already exists and you requested exclusive
// behavior, then this function will fail and return an EEXIST error. You may
// request that the newly opened file (relevant in non-exclusive mode) is
// automatically and atomically truncated to length 0 if it contained some data
// by passing the kOpen_Truncate option. 'permissions' are the file permissions
// that are assigned to a newly created file if it is actually created.
// @Concurrency: Safe
extern errno_t File_Create(const char* _Nonnull path, unsigned int mode, FilePermissions permissions, int* _Nonnull ioc);

// Opens an already existing file located at the filesystem location 'path'.
// Returns an error if the file does not exist or the caller lacks the necessary
// permissions to successfully open the file. 'mode' specifies whether the file
// should be opened for reading and/or writing. 'kOpen_Append' may be passed in
// addition to 'kOpen_Write' to force the system to always append any newly written
// data to the file. The file position is disregarded by the write function(s) in
// this case.
// @Concurrency: Safe
extern errno_t File_Open(const char* _Nonnull path, unsigned int mode, int* _Nonnull ioc);


// Returns the current file position. This is the position at which the next
// read or write operation will start.
// @Concurrency: Safe
extern errno_t File_GetPosition(int ioc, off_t* _Nonnull pos);

// Sets the current file position. Note that the file position may be set to a
// value past the current file size. Doing this implicitly expands the size of
// the file to encompass the new file position. The byte range between the old
// end of file and the new end of file is automatically filled with zero bytes.
// @Concurrency: Safe
extern errno_t File_Seek(int ioc, off_t offset, off_t* _Nullable oldpos, int whence);


// Truncates the file at the filesystem location 'path'. If the new length is
// greater than the size of the existing file, then the file is expanded and the
// newly added data range is zero-filled. If the new length is less than the
// size of the existing file, then the excess data is removed and the size of
// the file is set to the new length.
// @Concurrency: Safe
extern errno_t File_Truncate(const char* _Nonnull path, off_t length);


// Returns meta-information about the file located at the filesystem location 'path'.
// @Concurrency: Safe
extern errno_t File_GetInfo(const char* _Nonnull path, FileInfo* _Nonnull info);

// Updates the meta-information about the file located at the filesystem location
// 'path'. Note that only those pieces of the meta-information are modified for
// which the corresponding flag in 'info.modify' is set.
// @Concurrency: Safe
extern errno_t File_SetInfo(const char* _Nonnull path, MutableFileInfo* _Nonnull info);


// Checks whether the file at the filesystem location 'path' exists and whether
// it is accessible according to 'mode'. A suitable error is returned otherwise.
// @Concurrency: Safe
extern errno_t File_CheckAccess(const char* _Nonnull path, AccessMode mode);

// Deletes the file or (empty) directory located at the filesystem location 'path'.
// Note that this function deletes empty directories only.
// @Concurrency: Safe
extern errno_t File_Unlink(const char* _Nonnull path);

// Renames a file or directory located at the filesystem location 'oldpath' to
// the new name and filesystem location at 'newpath'. Both the old and the new
// filesystem location must reside in the same filesystem instance.
// @Concurrency: Safe
extern errno_t File_Rename(const char* _Nonnull oldpath, const char* _Nonnull newpath);


// Similar to File_Truncate() but operates on the open file identified by 'ioc'.
// @Concurrency: Safe
extern errno_t FileChannel_Truncate(int ioc, off_t length);


// Similar to File_GetInfo() but operates on the open file identified by 'ioc'.
// @Concurrency: Safe
extern errno_t FileChannel_GetInfo(int ioc, FileInfo* _Nonnull info);

// Similar to File_SetInfo() but operates on the open file identified by 'ioc'.
// @Concurrency: Safe
extern errno_t FileChannel_SetInfo(int ioc, MutableFileInfo* _Nonnull info);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_FILE_H */
