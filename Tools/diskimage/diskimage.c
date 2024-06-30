//
//  diskimage.c
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <klib/klib.h>
#include <filesystem/serenafs/SerenaFS.h>
#include <clap.h>

FilePermissions gDefaultDirPermissions;
FilePermissions gDefaultFilePermissions;
User gDefaultUser;

char gBuffer[4096];


static const char* gArgv_Zero = "";

static void vfatal(const char* fmt, va_list ap)
{
    clap_verror(gArgv_Zero, fmt, ap);
    exit(EXIT_FAILURE);
    // NOT REACHED
}

static void fatal(const char* fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfatal(fmt, ap);
    va_end(ap);
}


static errno_t formatDiskImage(DiskDriverRef _Nonnull pDisk)
{
    return SerenaFS_FormatDrive(pDisk, gDefaultUser, gDefaultDirPermissions);
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

static errno_t createADFDiskDriver(const DiskImageFormat* diskImageFormat, DiskDriverRef *pOutDriver)
{
    return DiskDriver_Create(diskImageFormat, pOutDriver);
}

static void createDiskImage(const char* pRootPath, const char* pDstPath, const DiskImageFormat* diskImageFormat)
{
    decl_try_err();
    DiskDriverRef pDisk = NULL;
    FilesystemRef pFS = NULL;
    InodeRef pRootDir = NULL;
    uint32_t dummyParam = 0;

    try(createADFDiskDriver(diskImageFormat, &pDisk));
    
    try(formatDiskImage(pDisk));

    try(SerenaFS_Create((SerenaFSRef*)&pFS));
    try(Filesystem_OnMount(pFS, pDisk, &dummyParam, 0));

    di_iterate_directory_callbacks cb;
    cb.context = pFS;
    cb.beginDirectory = (di_begin_directory_callback)beginDirectory;
    cb.endDirectory = (di_end_directory_callback)endDirectory;
    cb.file = (di_file_callback)copyFile;

    try(Filesystem_AcquireRootDirectory(pFS, &pRootDir));
    try(di_iterate_directory(pRootPath, &cb, pRootDir));
    Filesystem_RelinquishNode(pFS, pRootDir);
    try(Filesystem_OnUnmount(pFS));

    try(DiskDriver_WriteToPath(pDisk, pDstPath));
    
    Object_Release(pFS);
    Object_Release(pDisk);
    return;

catch:
    fatal(strerror(err));
}


////////////////////////////////////////////////////////////////////////////////
// main
////////////////////////////////////////////////////////////////////////////////

DiskImageFormat gDiskImageFormats[] = {
    {"adf-dd", kDiskImage_Amiga_DD_Floppy, 512, 11 * 2 * 80},   // 880KB
    {"adf-hd", kDiskImage_Amiga_HD_Floppy, 512, 22 * 2 * 80},   // 1.7MB
    {"smg", kDiskImage_Serena, 512, 128},
    {NULL, 0, 0, 0}
};

static int parseDiskFormat(const char* _Nonnull proc_name, const struct clap_param_t* _Nonnull param, unsigned int eo, const char* _Nonnull arg)
{
    long long size = 0ll;
    char* pptr = NULL;
    DiskImageFormat* de = &gDiskImageFormats[0];

    while (de->name) {
        if (!strcmp(arg, de->name)) {
            *(DiskImageFormat**)param->value = de;
            return EXIT_SUCCESS;
        }
        de++;
    }

    clap_param_error(proc_name, param, eo, "unknown disk image type '%s", arg);

    return EXIT_FAILURE;
}

// One of:
// 26326    (positive integer)
// 880k     (power-of-two unit postfix: k, K, m, M, g, G, t, T)
static int parseDiskSize(const char* _Nonnull proc_name, const struct clap_param_t* _Nonnull param, unsigned int eo, const char* _Nonnull arg)
{
    long long size = 0ll;
    char* pptr = NULL;

    errno = 0;
    size = strtoll(arg, &pptr, 10);
    if (pptr != arg) {
        // Some numeric input
        if (errno == 0) {
            long long mtp = 1ll;
            long long limit = LLONG_MAX;

            switch (*pptr) {
                case 'k':
                case 'K':   pptr++; mtp = 1024ll; break;
                case 'm':
                case 'M':   pptr++; mtp = 1024ll * 1024ll; break;
                case 'g':
                case 'G':   pptr++; mtp = 1024ll * 1024ll * 1024ll; break;
                case 't':
                case 'T':   pptr++; mtp = 1024ll * 1024ll * 1024ll * 1024ll; break;
                    break;

                default:
                    break;
            }

            if (*pptr != '\0') {
                clap_param_error(proc_name, param, eo, "unknown disk size multiplier '%s'", pptr);
                return EXIT_FAILURE;
            }

            limit /= mtp;
            if (size >= 0ll && size <= limit) {
                long long r = size * mtp;

                if (r <= SIZE_MAX) {
                    *(size_t*)param->value = r;
                    return EXIT_SUCCESS;
                }
            }
        }

        clap_param_error(proc_name, param, eo, "disk size too large");
    }
    else {
        // Syntax error
        clap_param_error(proc_name, param, eo, "not a valid disk size '%s", arg);
    }

    return EXIT_FAILURE;
}


clap_string_array_t paths = {NULL, 0};
const char* cmd_id = "";
DiskImageFormat* disk_format = NULL;
size_t disk_size = 0;       // disk size as specified by user; 0 by default -> use the disk format defined size

CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("diskimage <command> ..."),

    CLAP_REQUIRED_COMMAND("create", &cmd_id, "<root_path> <dimg_path>", "Creates a SerenaFS formatted disk image file 'dimg_path' which stores a recursive copy of the directory hierarchy and files located at 'root_path'."),
        CLAP_VALUE('d', "disk", &disk_format, parseDiskFormat, "Specify the format of the generated disk image (default: Amiga DD)"),
        CLAP_VALUE('s', "size", &disk_size, parseDiskSize, "Set the size of the disk image (default: depends on the disk image format)"),
        CLAP_VARARG(&paths)
);


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
    gArgv_Zero = argv[0];
    clap_parse(0, params, argc, argv);

    init();

    if (!strcmp(argv[1], "create")) {
        if (paths.count != 2) {
            fatal("expected a disk image and root path");
            return EXIT_FAILURE;
        }


        if (disk_format == NULL) {
            disk_format = &gDiskImageFormats[0];
        }
        
        if (disk_size > 0) {
            disk_format->blocksPerDisk = disk_size / disk_format->blockSize;
            if (disk_format->blocksPerDisk * disk_format->blockSize < disk_size) {
                disk_format->blocksPerDisk++;
            }
        }

        createDiskImage(paths.strings[0], paths.strings[1], disk_format);
    }

    return EXIT_SUCCESS;
}
