//
//  FileManager.h
//  kernel
//
//  Created by Dietmar Planitzer on 12/16/24.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef FileManager_h
#define FileManager_h

#include <klib/klib.h>
#include <kobj/Object.h>
#include <System/File.h>
#include <User.h>


typedef struct FileManager {
    FileHierarchyRef _Nonnull   fileHierarchy;

    InodeRef _Nonnull           rootDirectory;
    InodeRef _Nonnull           workingDirectory;
    
    FilePermissions             fileCreationMask;   // Mask of file permissions that should be filtered out from user supplied permissions when creating a file system object
    User                        realUser;           // User identity inherited from the parent process / set at spawn time
} FileManager;


extern void FileManager_Init(FileManagerRef _Nonnull self, FileHierarchyRef _Nonnull pFileHierarchy, User user, InodeRef _Nonnull pRootDir, InodeRef _Nonnull pWorkingDir, FilePermissions fileCreationMask);
extern void FileManager_Deinit(FileManagerRef _Nonnull self);

extern UserId FileManager_GetRealUserId(FileManagerRef _Nonnull self);


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
extern errno_t FileManager_CreateDirectory(FileManagerRef _Nonnull self, const char* _Nonnull path, FilePermissions permissions);

// Opens the directory at the given path and returns an I/O channel that represents
// the open directory.
extern errno_t FileManager_OpenDirectory(FileManagerRef _Nonnull self, const char* _Nonnull path, IOChannelRef _Nullable * _Nonnull pOutChannel);


//
// Files
//

// Returns the file creation mask of the receiver. Bits cleared in this mask
// should be removed from the file permissions that user space sent to create a
// file system object (note that this is the compliment of umask).
extern FilePermissions FileManager_GetFileCreationMask(FileManagerRef _Nonnull self);

// Sets the file creation mask of the receiver.
extern void FileManager_SetFileCreationMask(FileManagerRef _Nonnull self, FilePermissions mask);

// Creates a file in the given filesystem location.
extern errno_t FileManager_CreateFile(FileManagerRef _Nonnull self, const char* _Nonnull pPath, unsigned int mode, FilePermissions permissions, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Opens the given file or named resource. Opening directories is handled by the
// Process_OpenDirectory() function.
extern errno_t FileManager_OpenFile(FileManagerRef _Nonnull self, const char* _Nonnull pPath, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Opens an executable file.
extern errno_t FileManager_OpenExecutable(FileManagerRef _Nonnull self, const char* _Nonnull path, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Creates a new directory. 'permissions' are the file permissions that should be
// assigned to the new directory (modulo the file creation mask).
extern errno_t FileManager_CreateDirectory(FileManagerRef _Nonnull self, const char* _Nonnull pPath, FilePermissions permissions);

// Opens the directory at the given path and returns an I/O channel that represents
// the open directory.
extern errno_t FileManager_OpenDirectory(FileManagerRef _Nonnull self, const char* _Nonnull pPath, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Returns information about the file at the given path.
extern errno_t FileManager_GetFileInfo(FileManagerRef _Nonnull self, const char* _Nonnull pPath, FileInfo* _Nonnull pOutInfo);

// Same as above but with respect to the given I/O channel.
extern errno_t FileManager_GetFileInfoFromIOChannel(FileManagerRef _Nonnull self, IOChannelRef _Nonnull pChannel, FileInfo* _Nonnull pOutInfo);

// Modifies information about the file at the given path.
extern errno_t FileManager_SetFileInfo(FileManagerRef _Nonnull self, const char* _Nonnull pPath, MutableFileInfo* _Nonnull pInfo);

// Same as above but with respect to the given I/O channel.
extern errno_t FileManager_SetFileInfoFromIOChannel(FileManagerRef _Nonnull self, IOChannelRef _Nonnull pChannel, MutableFileInfo* _Nonnull pInfo);

// Sets the length of an existing file. The file may either be reduced in size
// or expanded.
extern errno_t FileManager_TruncateFile(FileManagerRef _Nonnull self, const char* _Nonnull pPath, FileOffset length);

// Same as above but the file is identified by the given I/O channel.
extern errno_t FileManager_TruncateFileFromIOChannel(FileManagerRef _Nonnull self, IOChannelRef _Nonnull pChannel, FileOffset length);

// Returns EOK if the given file is accessible assuming the given access mode;
// returns a suitable error otherwise. If the mode is 0, then a check whether the
// file exists at all is executed.
extern errno_t FileManager_CheckAccess(FileManagerRef _Nonnull self, const char* _Nonnull pPath, AccessMode mode);

// Unlinks the inode at the path 'pPath'.
extern errno_t FileManager_Unlink(FileManagerRef _Nonnull self, const char* _Nonnull pPath);

// Renames the file or directory at 'pOldPath' to the new location 'pNewPath'.
extern errno_t FileManager_Rename(FileManagerRef _Nonnull self, const char* pOldPath, const char* pNewPath);


//
// Filesystems
//

extern errno_t FileManager_Mount(FileManagerRef _Nonnull self, const char* _Nonnull containerPath, const char* _Nonnull atDirPath, const void* _Nullable params, size_t paramsSize);

extern errno_t FileManager_Unmount(FileManagerRef _Nonnull self, const char* _Nonnull atDirPath, uint32_t options);

#endif /* FileManager_h */
