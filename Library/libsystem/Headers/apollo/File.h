//
//  File.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_FILE_H
#define _SYS_FILE_H 1

#include <apollo/_cmndef.h>
#include <apollo/_syslimits.h>
#include <apollo/Error.h>
#include <apollo/types.h>

__CPP_BEGIN

// The Inode type.
enum {
    kFileType_RegularFile = 0,  // A regular file that stores data
    kFileType_Directory,        // A directory which stores information about child nodes
};

// File permissions. Every file and directory has 3 sets of permissions associated
// with it (also knows as "permission scopes"):
// Owner (of the file)
// Group (the file is associated with)
// Anyone else
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
// Note that a FilePermission value holds permission bits for all three permission
// scopes.
enum {
    kFilePermission_Read = 0x04,
    kFilePermission_Write = 0x02,
    kFilePermission_Execute = 0x01
};

enum {
    kFilePermissionScope_BitWidth = 3,

    kFilePermissionScope_User = 2*kFilePermissionScope_BitWidth,
    kFilePermissionScope_Group = kFilePermissionScope_BitWidth,
    kFilePermissionScope_Other = 0,

    kFilePermissionScope_Mask = 0x07,
};

// Creates a FilePermission value with permissions for a user, group and other
// permission scope.
#define FilePermissions_Make(__user, __group, __other) \
  (((__user) & kFilePermissionScope_Mask) << kFilePermissionScope_User) \
| (((__group) & kFilePermissionScope_Mask) << kFilePermissionScope_Group) \
| (((__other) & kFilePermissionScope_Mask) << kFilePermissionScope_Other)

// Creates a FilePermission value from a POSIX style octal number. This number
// is expected to be a 3 digit number where each digit represents one of the
// permission scopes.
#define FilePermissions_MakeFromOctal(__3_x_3_octal) \
    (__3_x_3_octal)
    
// Returns the permission bits of '__permission' that correspond to the
// permissions scope '__scope'.
#define FilePermissions_Get(__permissions, __scope) \
(((__permissions) >> (__scope)) & kFilePermissionScope_Mask)

// Replaces the permission bits of the scope '__scope' in '__permission' with the
// permission bits '__bits'. 
#define FilePermissions_Set(__permissions, __scope, __bits) \
((__permissions) & ~(kFilePermissionsScope_Mask << (__scope))) | (((__bits) & kFilePermissionsScope_Mask) << (__scope))


typedef struct FileInfo {
    struct _time_interval_t accessTime;
    struct _time_interval_t modificationTime;
    struct _time_interval_t statusChangeTime;
    FileOffset              size;
    UserId                  uid;
    GroupId                 gid;
    FilePermissions         permissions;
    FileType                type;
    char                    reserved;
    long                    linkCount;
    FilesystemId            filesystemId;
    InodeId                 inodeId;
} FileInfo;

enum {
    kModifyFileInfo_AccessTime = 1,
    kModifyFileInfo_ModificationTime = 2,
    kModifyFileInfo_UserId = 4,
    kModifyFileInfo_GroupId = 8,
    kModifyFileInfo_Permissions = 16
};

typedef struct MutableFileInfo {
    unsigned long           modify;
    struct _time_interval_t accessTime;
    struct _time_interval_t modificationTime;
    UserId                  uid;
    GroupId                 gid;
    FilePermissions         permissions;
    unsigned short          permissionsModifyMask;  // Only modify permissions whose bit is set here
} MutableFileInfo;


typedef struct DirectoryEntry {
    InodeId     inodeId;
    char        name[__PATH_COMPONENT_MAX];
} DirectoryEntry;


enum {
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
enum {
    kSeek_Set = 0,      // Set the file position to 'offset'
    kSeek_Current,      // Add 'offset' to the current file position
    kSeek_End           // Add 'offset' to the end of the file
};


#define PATH_MAX __PATH_MAX
#define NAME_MAX __PATH_COMPONENT_MAX


#if !defined(__KERNEL__)

extern errno_t File_Create(const char* path, unsigned int options, FilePermissions permissions, int* fd);

extern errno_t File_Open(const char *path, unsigned int options, int* fd);

extern errno_t File_GetPosition(int fd, FileOffset* pos);
extern errno_t File_Seek(int fd, FileOffset offset, FileOffset *oldpos, int whence);

extern errno_t File_Truncate(const char *path, FileOffset length);

extern errno_t File_GetInfo(const char* path, FileInfo* info);
extern errno_t File_SetInfo(const char* path, MutableFileInfo* info);

extern errno_t File_CheckAccess(const char* path, AccessMode mode);
extern errno_t File_Unlink(const char* path);    // deletes files and (empty) directories
extern errno_t File_Rename(const char* oldpath, const char* newpath);


extern errno_t FileChannel_Truncate(int fd, FileOffset length);

extern errno_t FileChannel_GetInfo(int fd, FileInfo* info);
extern errno_t FileChannel_SetInfo(int fd, MutableFileInfo* info);


extern errno_t Directory_Create(const char* path, FilePermissions mode);
extern errno_t Directory_Open(const char* path, int* fd);
extern errno_t Directory_Read(int fd, DirectoryEntry* entries, size_t nEntriesToRead, ssize_t* nReadEntries);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_FILE_H */
