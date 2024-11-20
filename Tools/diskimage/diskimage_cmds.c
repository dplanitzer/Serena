//
//  diskimage_cmds.c
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include "RamFSContainer.h"
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <klib/klib.h>
#include <filesystem/serenafs/SerenaFS.h>


//
// diskimage create
//

FilePermissions gDefaultDirPermissions;
FilePermissions gDefaultFilePermissions;
User gDefaultUser;

char gBuffer[4096];


static errno_t formatDiskImage(RamFSContainerRef _Nonnull pContainer)
{
    return SerenaFS_FormatDrive((FSContainerRef)pContainer, gDefaultUser, gDefaultDirPermissions);
}

static errno_t beginDirectory(FilesystemRef _Nonnull pFS, const char* _Nonnull pBasePath, const di_direntry* _Nonnull pEntry, InodeRef _Nonnull pParentNode, InodeRef* pOutDirInode)
{
    PathComponent pc;

    pc.name = pEntry->name;
    pc.count = strlen(pc.name);

    return Filesystem_CreateNode(pFS, kFileType_Directory, gDefaultUser, gDefaultDirPermissions, pParentNode, &pc, NULL, pOutDirInode);
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
    if (FilePermissions_Has(pEntry->permissions, kFilePermissionsClass_User, kFilePermission_Execute)) {
        FilePermissions_Add(permissions, kFilePermissionsClass_User, kFilePermission_Execute);
    }
    if (FilePermissions_Has(pEntry->permissions, kFilePermissionsClass_Group, kFilePermission_Execute)) {
        FilePermissions_Add(permissions, kFilePermissionsClass_Group, kFilePermission_Execute);
    }
    if (FilePermissions_Has(pEntry->permissions, kFilePermissionsClass_Other, kFilePermission_Execute)) {
        FilePermissions_Add(permissions, kFilePermissionsClass_Other, kFilePermission_Execute);
    }


    try(Filesystem_CreateNode(pFS, kFileType_RegularFile, gDefaultUser, permissions, pDirInode, &pc, NULL, &pDstFile));

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

static errno_t createADFDiskContainer(const DiskImageFormat* diskImageFormat, RamFSContainerRef *pOutContainer)
{
    return RamFSContainer_Create(diskImageFormat, pOutContainer);
}

static void initDefaultPermissionsAndUser(void)
{
    const FilePermissions dirOwnerPerms = kFilePermission_Read | kFilePermission_Write | kFilePermission_Execute;
    const FilePermissions dirOtherPerms = kFilePermission_Read | kFilePermission_Execute;
    gDefaultDirPermissions = FilePermissions_Make(dirOwnerPerms, dirOtherPerms, dirOtherPerms);

    const FilePermissions fileOwnerPerms = kFilePermission_Read | kFilePermission_Write;
    const FilePermissions fileOtherPerms = kFilePermission_Read;
    gDefaultFilePermissions = FilePermissions_Make(fileOwnerPerms, fileOtherPerms, fileOtherPerms);

    gDefaultUser = kUser_Root;
}

void cmd_createDiskImage(const char* pRootPath, const char* pDstPath, const DiskImageFormat* diskImageFormat)
{
    decl_try_err();
    RamFSContainerRef pContainer = NULL;
    FilesystemRef pFS = NULL;
    InodeRef pRootDir = NULL;
    uint32_t dummyParam = 0;

    initDefaultPermissionsAndUser();

    try(createADFDiskContainer(diskImageFormat, &pContainer));
    
    try(formatDiskImage(pContainer));

    try(SerenaFS_Create((FSContainerRef)pContainer, (SerenaFSRef*)&pFS));
    try(Filesystem_Start(pFS, &dummyParam, 0));

    di_iterate_directory_callbacks cb;
    cb.context = pFS;
    cb.beginDirectory = (di_begin_directory_callback)beginDirectory;
    cb.endDirectory = (di_end_directory_callback)endDirectory;
    cb.file = (di_file_callback)copyFile;

    try(Filesystem_AcquireRootDirectory(pFS, &pRootDir));
    try(di_iterate_directory(pRootPath, &cb, pRootDir));
    Filesystem_RelinquishNode(pFS, pRootDir);
    try(Filesystem_Stop(pFS));

    try(RamFSContainer_WriteToPath(pContainer, pDstPath));
    
    Object_Release(pFS);
    Object_Release(pContainer);
    return;

catch:
    fatal(strerror(err));
}
