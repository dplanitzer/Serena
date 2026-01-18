//
//  FileManager.h
//  kernel
//
//  Created by Dietmar Planitzer on 12/16/24.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef FileManager_h
#define FileManager_h

#include <kobj/Object.h>
#include <kpi/fs.h>
#include <kpi/stat.h>
#include <kpi/uid.h>
#ifndef __DISKIMAGE__
#include <sys/mount.h>
#endif


typedef struct FileManager {
    FileHierarchyRef _Nonnull   fileHierarchy;

    InodeRef _Nonnull           rootDirectory;
    InodeRef _Nonnull           workingDirectory;
    
    mode_t                      umask;      // Mask of file permissions that should be filtered out from user supplied permissions when creating a file system object
    uid_t                       ruid;       // Real user identity inherited from the parent process / set at spawn time
    gid_t                       rgid;
} FileManager;


extern void FileManager_Init(FileManagerRef _Nonnull self, FileHierarchyRef _Nonnull pFileHierarchy, uid_t uid, gid_t gid, InodeRef _Nonnull pRootDir, InodeRef _Nonnull pWorkingDir, mode_t umask);
extern void FileManager_Deinit(FileManagerRef _Nonnull self);

extern uid_t FileManager_GetRealUserId(FileManagerRef _Nonnull self);
extern gid_t FileManager_GetRealGroupId(FileManagerRef _Nonnull self);


//
// Directories
//

// Sets the receiver's root directory to the given path. Note that the path must
// point to a directory that is a child or the current root directory of the
// process.
extern errno_t FileManager_SetRootDirectoryPath(FileManagerRef _Nonnull self, const char* _Nonnull path);

// Sets the receiver's current working directory to the given path.
extern errno_t FileManager_SetWorkingDirectoryPath(FileManagerRef _Nonnull self, const char* _Nonnull path);

// Returns the current working directory in the form of a path. The path is
// written to the provided buffer 'pBuffer'. The buffer size must be at least as
// large as length(path) + 1.
extern errno_t FileManager_GetWorkingDirectoryPath(FileManagerRef _Nonnull self, char* _Nonnull pBuffer, size_t bufferSize);

// Creates a new directory. 'permissions' are the file permissions that should be
// assigned to the new directory (modulo the file creation mask).
extern errno_t FileManager_CreateDirectory(FileManagerRef _Nonnull self, const char* _Nonnull path, mode_t permissions);

// Opens the directory at the given path and returns an I/O channel that represents
// the open directory.
extern errno_t FileManager_OpenDirectory(FileManagerRef _Nonnull self, const char* _Nonnull path, IOChannelRef _Nullable * _Nonnull pOutChannel);


//
// Files
//

// Sets the umask. Bits set in 'mask' are cleared in the mode used to create a
// file. Returns the old umask.
extern mode_t FileManager_UMask(FileManagerRef _Nonnull self, mode_t mask);

// Returns the current umask.
#define FileManager_GetUMask(__self) \
(__self)->umask

// Creates a file in the given filesystem location.
extern errno_t FileManager_CreateFile(FileManagerRef _Nonnull self, const char* _Nonnull pPath, int oflags, mode_t mode, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Opens the given file or named resource. Opening directories is handled by the
// FileManager_OpenDirectory() function.
extern errno_t FileManager_OpenFile(FileManagerRef _Nonnull self, const char* _Nonnull pPath, int oflags, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Creates a new directory. 'mode' are the file permissions that should be
// assigned to the new directory (modulo the file creation mask).
extern errno_t FileManager_CreateDirectory(FileManagerRef _Nonnull self, const char* _Nonnull pPath, mode_t mode);

// Opens the directory at the given path and returns an I/O channel that represents
// the open directory.
extern errno_t FileManager_OpenDirectory(FileManagerRef _Nonnull self, const char* _Nonnull pPath, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Returns information about the file at the given path.
extern errno_t FileManager_GetFileInfo(FileManagerRef _Nonnull self, const char* _Nonnull pPath, struct stat* _Nonnull pOutInfo);

extern errno_t FileManager_SetFileMode(FileManagerRef _Nonnull self, const char* _Nonnull path, mode_t mode);

extern errno_t FileManager_SetFileOwner(FileManagerRef _Nonnull self, const char* _Nonnull path, uid_t uid, gid_t gid);

extern errno_t FileManager_SetFileTimestamps(FileManagerRef _Nonnull self, const char* _Nonnull path, const struct timespec times[_Nullable 2]);

// Sets the length of an existing file. The file may either be reduced in size
// or expanded.
extern errno_t FileManager_TruncateFile(FileManagerRef _Nonnull self, const char* _Nonnull path, off_t length);

// Returns EOK if the given file is accessible assuming the given access mode;
// returns a suitable error otherwise. If the mode is 0, then a check whether the
// file exists at all is executed.
extern errno_t FileManager_CheckAccess(FileManagerRef _Nonnull self, const char* _Nonnull path, int mode);

// Unlinks the inode at the path 'pPath'.
extern errno_t FileManager_Unlink(FileManagerRef _Nonnull self, const char* _Nonnull path, int mode);

// Renames the file or directory at 'pOldPath' to the new location 'pNewPath'.
extern errno_t FileManager_Rename(FileManagerRef _Nonnull self, const char* pOldPath, const char* pNewPath);


//
// Filesystems
//
#ifndef __DISKIMAGE__
extern errno_t FileManager_Mount(FileManagerRef _Nonnull self, const char* _Nonnull objectType, const char* _Nonnull objectName, const char* _Nonnull atDirPath, const char* _Nonnull params);

extern errno_t FileManager_Unmount(FileManagerRef _Nonnull self, const char* _Nonnull atDirPath, UnmountOptions options);

extern errno_t FileManager_GetFilesystemDiskPath(FileManagerRef _Nonnull self, fsid_t fsid, char* _Nonnull buf, size_t bufSize);
#endif  /* __DISKIMAGE__ */


//
// Internal
//

extern errno_t _FileManager_OpenFile(FileManagerRef _Nonnull self, InodeRef _Nonnull _Locked pFile, int oflags);

#endif /* FileManager_h */
