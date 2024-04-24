//
//  diskimage.c
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include <stdio.h>
#include <stdlib.h>
#include <klib/klib.h>
#include <filesystem/FilesystemManager.h>
#include <filesystem/serenafs/SerenaFS.h>

FilePermissions gDefaultDirPermissions;
FilePermissions gDefaultFilePermissions;
User gDefaultUser;

char gBuffer[4096];


static void failed(errno_t err)
{
    printf("Error: %d\n", err);
    exit(EXIT_FAILURE);
}

static errno_t formatDiskImage(DiskDriverRef _Nonnull pDisk)
{
    return SerenaFS_FormatDrive(pDisk, gDefaultUser, gDefaultDirPermissions);
}

static errno_t beginDirectory(FilesystemRef _Nonnull pFS, const char* _Nonnull pBasePath, const di_direntry* _Nonnull pEntry, InodeRef _Nonnull pParentNode, InodeRef* pOutDirInode)
{
    decl_try_err();
    PathComponent pc;

    pc.name = pEntry->name;
    pc.count = strlen(pc.name);

    try(Filesystem_CreateDirectory(pFS, &pc, pParentNode, gDefaultUser, gDefaultDirPermissions));
    try(Filesystem_AcquireNodeForName(pFS, pParentNode, &pc, gDefaultUser, pOutDirInode));
    return EOK;

catch:
    return err;
}

static errno_t endDirectory(FilesystemRef _Nonnull pFS, InodeRef pDirInode)
{
    Filesystem_RelinquishNode(pFS, pDirInode);
    return EOK;
}

static errno_t copyFile(FilesystemRef _Nonnull pFS, const char* _Nonnull pBasePath, const di_direntry* _Nonnull pEntry, InodeRef pDirInode)
{
    decl_try_err();
    const unsigned int mode = kOpen_Write | kOpen_Exclusive;
    FileOffset dstFileOffset = 0ll;
    InodeRef pDstFile = NULL;
    FILE* pSrcFile = NULL;

    PathComponent pc;
    pc.name = pEntry->name;
    pc.count = strlen(pc.name);


    FilePermissions permissions = gDefaultFilePermissions;
    if (FilePermissions_Has(pEntry->permissions, kFilePermissionsScope_User, kFilePermission_Execute)) {
        FilePermissions_Add(permissions, kFilePermissionsScope_User, kFilePermission_Execute);
    }
    if (FilePermissions_Has(pEntry->permissions, kFilePermissionsScope_Group, kFilePermission_Execute)) {
        FilePermissions_Add(permissions, kFilePermissionsScope_Group, kFilePermission_Execute);
    }
    if (FilePermissions_Has(pEntry->permissions, kFilePermissionsScope_Other, kFilePermission_Execute)) {
        FilePermissions_Add(permissions, kFilePermissionsScope_Other, kFilePermission_Execute);
    }


    try(Filesystem_CreateFile(pFS, &pc, pDirInode, gDefaultUser, mode, permissions, &pDstFile));

    try(di_concat_path(pBasePath, pEntry->name, gBuffer, sizeof(gBuffer)));
    try_null(pSrcFile, fopen(gBuffer, "rb"), EIO);
    setvbuf(pSrcFile, NULL, _IONBF, 0);

    while (!feof(pSrcFile)) {
        const size_t r = fread(gBuffer, sizeof(char), sizeof(gBuffer), pSrcFile);
        ssize_t nBytesWritten = 0;

        if (r < sizeof(gBuffer) && ferror(pSrcFile) != 0) {
            throw(EIO);
        }

        if (r > 0) {
            try(Filesystem_WriteFile(pFS, pDstFile, gBuffer, r, &dstFileOffset, &nBytesWritten));
        }
    }

catch:
    if (pDstFile) {
        Filesystem_RelinquishNode(pFS, pDstFile);
    }
    if (pSrcFile) {
        fclose(pSrcFile);
    }
    return err;
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
    try(FilesystemManager_Create(&gFilesystemManager));
    try(FilesystemManager_Mount(gFilesystemManager, pFS, pDisk, NULL, 0, NULL));

    di_iterate_directory_callbacks cb;
    cb.context = pFS;
    cb.beginDirectory = (di_begin_directory_callback)beginDirectory;
    cb.endDirectory = (di_end_directory_callback)endDirectory;
    cb.file = (di_file_callback)copyFile;

    try(Filesystem_AcquireRootNode(pFS, &rootInode));
    try(di_iterate_directory(pRootPath, &cb, rootInode));
    Filesystem_RelinquishNode(pFS, rootInode);
    try(FilesystemManager_Unmount(gFilesystemManager, pFS, false));

    try(DiskDriver_WriteToPath(pDisk, pDstPath));
    
    Object_Release(pFS);
    Object_Release(pDisk);
    return;

catch:
    failed(err);
}


////////////////////////////////////////////////////////////////////////////////

static void init(void)
{
    _RegisterClass(&kAnyClass);
    _RegisterClass(&kObjectClass);
    _RegisterClass(&kDiskDriverClass);
    _RegisterClass(&kFilesystemClass);
    _RegisterClass(&kSerenaFSClass);


    const FilePermissions dirOwnerPerms = kFilePermission_Read | kFilePermission_Write | kFilePermission_Execute;
    const FilePermissions dirOtherPerms = kFilePermission_Read | kFilePermission_Execute;
    gDefaultDirPermissions = FilePermissions_Make(dirOwnerPerms, dirOtherPerms, dirOtherPerms);

    const FilePermissions fileOwnerPerms = kFilePermission_Read | kFilePermission_Write;
    const FilePermissions fileOtherPerms = kFilePermission_Read;
    gDefaultFilePermissions = FilePermissions_Make(fileOwnerPerms, fileOtherPerms, fileOtherPerms);

    gDefaultUser = kUser_Root;
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
