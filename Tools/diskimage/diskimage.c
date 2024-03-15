//
//  diskimage.c
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include <stdlib.h>
#include <klib/klib.h>
#include <filesystem/FilesystemManager.h>
#include <filesystem/serenafs/SerenaFS.h>

FilePermissions gDefaultDirPermissions;
FilePermissions gDefaultFilePermissions;
User gDefaultUser;


static void failed(errno_t err)
{
    printf("Error: %d\n", err);
    exit(EXIT_FAILURE);
}

static errno_t formatDiskImage(DiskDriverRef _Nonnull pDisk)
{
    return SerenaFS_FormatDrive(pDisk, gDefaultUser, gDefaultDirPermissions);
}

static errno_t beginDirectory(FilesystemRef _Nonnull pFS, const di_direntry* _Nonnull pEntry, InodeRef _Nonnull pParentNode, InodeRef* pOutDirInode)
{
    PathComponent pc;

    pc.name = pEntry->name;
    pc.count = strlen(pc.name);

    printf("  %s   <DIR>\n", pEntry->name);
    return Filesystem_CreateDirectory(pFS, &pc, pParentNode, gDefaultUser, gDefaultDirPermissions);
}

static errno_t endDirectory(FilesystemRef _Nonnull pFS, InodeRef pDirInode)
{
    Filesystem_RelinquishNode(pFS, pDirInode);
    return EOK;
}

static errno_t copyFile(FilesystemRef _Nonnull pFS, const di_direntry* _Nonnull pEntry, InodeRef pDirInode)
{
    // XXX implement me
    //printf("  %s   %llu bytes\n", pEntry->name, pEntry->fileSize);
    return EOK;
}

static void createDiskImage(const char* pRootPath, const char* pDstPath)
{
    decl_try_err();
    DiskDriverRef pDisk = NULL;
    FilesystemRef pFS = NULL;
    InodeRef rootInode = NULL;

    DiskDriver_Create(512, 128, &pDisk);
    
    try(formatDiskImage(pDisk));

    try(SerenaFS_Create((SerenaFSRef*)&pFS));
    try(FilesystemManager_Create(pFS, pDisk, &gFilesystemManager));

    di_iterate_directory_callbacks cb;
    cb.context = pFS;
    cb.beginDirectory = (di_begin_directory_callback)beginDirectory;
    cb.endDirectory = (di_end_directory_callback)endDirectory;
    cb.file = (di_file_callback)copyFile;

    try(Filesystem_AcquireRootNode(pFS, &rootInode));
    try(di_iterate_directory(pRootPath, &cb, rootInode));
    Filesystem_RelinquishNode(pFS, rootInode);

    try(DiskDriver_WriteToPath(pDisk, pDstPath));
    
    Object_Release(pFS);
    Object_Release(pDisk);
    printf("Done\n");
    return;

catch:
    failed(err);
}


////////////////////////////////////////////////////////////////////////////////

static void init(void)
{
    _RegisterClass(&kObjectClass);
    _RegisterClass(&kIOChannelClass);
    _RegisterClass(&kIOResourceClass);
    _RegisterClass(&kDiskDriverClass);
    _RegisterClass(&kFileClass);
    _RegisterClass(&kDirectoryClass);
    _RegisterClass(&kFilesystemClass);
    _RegisterClass(&kSerenaFSClass);


    const FilePermissions dirOwnerPerms = kFilePermission_Read | kFilePermission_Write | kFilePermission_Execute;
    const FilePermissions dirOtherPerms = kFilePermission_Read | kFilePermission_Execute;
    gDefaultDirPermissions = FilePermissions_Make(dirOwnerPerms, dirOtherPerms, dirOtherPerms);

    const FilePermissions fileOwnerPerms = kFilePermission_Read | kFilePermission_Write;
    const FilePermissions fileOtherPerms = kFilePermission_Read;
    gDefaultFilePermissions = FilePermissions_Make(fileOwnerPerms, fileOtherPerms, fileOtherPerms);

    gDefaultUser = (User){0, 0};
}

int main(int argc, char* argv[])
{
    init();

    if (argc > 1) {
        if (!strcmp(argv[1], "create")) {
            if (argc > 3) {
                createDiskImage(argv[2], argv[3]);
                return EXIT_SUCCESS;
            }
        }
    }

    printf("diskimage <action> ...\n");
    printf("   create <root_path> <dimg_path> ...   Creates a SerenaFS formatted disk image file 'dimg_path' which stores a recursive copy of the directory hierarchy and files located at 'root_path'\n");

    return EXIT_FAILURE;
}
