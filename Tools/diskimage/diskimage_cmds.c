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
#include <driver/amiga/floppy/adf.h>
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

errno_t cmd_createDiskImage(const char* _Nonnull rootPath, const char* _Nonnull dmgPath, const DiskImageFormat* _Nonnull diskImageFormat)
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
    try(di_iterate_directory(rootPath, &cb, pRootDir));
    Filesystem_RelinquishNode(pFS, pRootDir);
    try(Filesystem_Stop(pFS));

    try(RamFSContainer_WriteToPath(pContainer, dmgPath));
    
    Object_Release(pFS);
    Object_Release(pContainer);

catch:
    return err;
}


//
// diskimage describe
//

errno_t cmd_describeDiskImage(const DiskImage* _Nonnull info)
{
    const char* formatName;

    switch (info->format) {
        case kDiskImage_Amiga_DD_Floppy:    formatName = "Amiga DD Floppy"; break;
        case kDiskImage_Amiga_HD_Floppy:    formatName = "Amiga HD Floppy"; break;
        case kDiskImage_Serena:             formatName = "Serena Disk Image"; break;
        default:                            abort(); break;
    }

    printf("Type: %s\n\n", formatName);

    if (info->format == kDiskImage_Serena) {
        printf("Logical Size: %zi Blocks\n", info->cylindersPerDisk);
        printf("Physical Size: %zi Blocks\n\n", info->physicalSize / info->bytesPerSector);
        printf("Sector Size: %ziB\n", info->bytesPerSector);
        printf("Disk Size:   %ziKB\n", info->physicalSize / 1024);
    }
    else {
        printf("Cylinders: %zi\n", info->cylindersPerDisk);
        printf("Heads:     %zi\n", info->headsPerCylinder);
        printf("Sectors:   %zi\n\n", info->sectorsPerTrack);
        printf("Sector Size: %ziB\n", info->bytesPerSector);
        printf("Disk Size:   %ziKB\n", info->physicalSize / 1024);
    }

    return EOK;
}


//
// diskimage get --sector=c:h:s
//

#define ADDR_FMT "%.8zx"

static void print_hex_line(size_t addr, const uint8_t* buf, size_t nbytes, size_t ncolumns)
{
    printf(ADDR_FMT"   ", addr);

    for (size_t i = 0; i < nbytes; i++) {
        printf("%.2hhx ", buf[i]);
    }
    for (size_t i = nbytes; i < ncolumns; i++) {
        fputs("   ", stdout);
    }
    
    fputs("  ", stdout);
    for (size_t i = 0; i < nbytes; i++) {
        const int ch = (isprint(buf[i])) ? buf[i] : '.';

        fputc(ch, stdout);
    }
    for (size_t i = nbytes; i < ncolumns; i++) {
        fputc(' ', stdout);
    }
}

static void print_hex_buffer(const char* _Nonnull buf, size_t bufSize)
{
    const size_t ncolumns = 16;
    size_t addr = 0;
    
    while (addr < bufSize) {
        const size_t nleft = bufSize - addr;
        const size_t nbytes = (nleft >= ncolumns) ? ncolumns : nleft;

        print_hex_line(addr, buf, nbytes, ncolumns);
        addr += ncolumns;
        buf += ncolumns;
        fputc('\n', stdout);
    }
}

static void print_binary_buffer(const char* _Nonnull buf, size_t bufSize)
{
    fwrite(buf, bufSize, 1, stdout);
}

static errno_t print_disk_slice(const char* _Nonnull dmgPath, const DiskImage* _Nonnull info, di_addr_t* _Nonnull addr, size_t sectorCount, bool isHex)
{
    decl_try_err();
    FILE* fp = NULL;
    char* buf = NULL;
    size_t lba;

    try(di_lba_from_disk_addr(&lba, info, addr));
    try_null(fp, fopen(dmgPath, "rb"), errno);
    try_null(buf, malloc(info->bytesPerSector * sectorCount), ENOMEM);
    fseek(fp, info->physicalOffset + info->bytesPerSector * lba, SEEK_SET);

    if (fread(buf, info->bytesPerSector, sectorCount, fp) < 1) {
        throw(EIO);
    }

    if (isHex) {
        print_hex_buffer(buf, info->bytesPerSector * sectorCount);
    }
    else {
        print_binary_buffer(buf, info->bytesPerSector * sectorCount);
    }

catch:
    if (fp) {
        fclose(fp);
    }
    free(buf);

    return err;
}

errno_t cmd_getDiskSlice(const char* _Nonnull dmgPath, const DiskImage* _Nonnull info, di_slice_t* _Nonnull slice, bool isHex)
{
    size_t sectorCount;
    
    switch (slice->type) {
        case di_slice_type_empty:
            break;

        case di_slice_type_sector:
            sectorCount = 1;
            break;

        case di_slice_type_track:
            sectorCount = info->sectorsPerTrack;
            break;

        default:
            abort();
    }

    return print_disk_slice(dmgPath, info, &slice->start, sectorCount, isHex);
}
