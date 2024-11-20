//
//  diskimage.c
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include "RamFSContainer.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <klib/klib.h>
#include <filesystem/serenafs/SerenaFS.h>
#include <clap.h>

static const char* gArgv_Zero = "";

void vfatal(const char* fmt, va_list ap)
{
    clap_verror(gArgv_Zero, fmt, ap);
    exit(EXIT_FAILURE);
    // NOT REACHED
}

void fatal(const char* fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfatal(fmt, ap);
    va_end(ap);
}


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
    _RegisterClass(&kFSContainerClass);
    _RegisterClass(&kRamFSContainerClass);
    _RegisterClass(&kFilesystemClass);
    _RegisterClass(&kContainerFilesystemClass);
    _RegisterClass(&kSerenaFSClass);
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
        
        if (disk_size > 0 && disk_format->format == kDiskImage_Serena) {
            disk_format->blocksPerDisk = disk_size / disk_format->blockSize;
            if (disk_format->blocksPerDisk * disk_format->blockSize < disk_size) {
                disk_format->blocksPerDisk++;
            }
        }

        cmd_createDiskImage(paths.strings[0], paths.strings[1], disk_format);
    }

    return EXIT_SUCCESS;
}
