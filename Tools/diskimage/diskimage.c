//
//  diskimage.c
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include "RamFSContainer.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <klib/klib.h>
#include <driver/amiga/floppy/adf.h>
#include <filesystem/SerenaDiskImage.h>
#include <filesystem/serenafs/SerenaFS.h>
#include <clap.h>
#include <System/ByteOrder.h>

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

errno_t di_describe_diskimage(const char* _Nonnull dmgPath, DiskImage* _Nonnull pOutInfo)
{
    decl_try_err();
    FILE* fp = NULL;
    long fileSize;
    SMG_Header smgHdr;

    try_null(fp, fopen(dmgPath, "r"), errno);
    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fileSize < sizeof(SMG_Header)) {
        throw(EINVAL);
    }
    if (fread(&smgHdr, sizeof(SMG_Header), 1, fp) != 1) {
        throw(errno);
    }

    if (smgHdr.signature == UInt32_HostToBig(SMG_SIGNATURE)) {
        pOutInfo->format = kDiskImage_Serena;
        pOutInfo->cylindersPerDisk = 1;
        pOutInfo->headsPerCylinder = 1;
        pOutInfo->sectorsPerTrack = UInt32_BigToHost(smgHdr.logicalBlockCount);
        pOutInfo->bytesPerSector = UInt32_BigToHost(smgHdr.blockSize);
        pOutInfo->physicalOffset = UInt32_BigToHost(smgHdr.headerSize);
        pOutInfo->physicalSize = UInt32_BigToHost(smgHdr.physicalBlockCount) * pOutInfo->bytesPerSector;
    }
    else if (fileSize == ADF_DD_CYLS_PER_DISK*ADF_DD_HEADS_PER_CYL*ADF_DD_SECS_PER_TRACK*ADF_SECTOR_DATA_SIZE) {
        pOutInfo->format = kDiskImage_Amiga_DD_Floppy;
        pOutInfo->cylindersPerDisk = ADF_DD_CYLS_PER_DISK;
        pOutInfo->headsPerCylinder = ADF_DD_HEADS_PER_CYL;
        pOutInfo->sectorsPerTrack = ADF_DD_SECS_PER_TRACK;
        pOutInfo->bytesPerSector = ADF_SECTOR_DATA_SIZE;
        pOutInfo->physicalOffset = 0;
        pOutInfo->physicalSize = pOutInfo->cylindersPerDisk * pOutInfo->headsPerCylinder * pOutInfo->sectorsPerTrack * pOutInfo->bytesPerSector;
    }
    else if (fileSize == ADF_HD_CYLS_PER_DISK*ADF_HD_HEADS_PER_CYL*ADF_HD_SECS_PER_TRACK*ADF_SECTOR_DATA_SIZE) {
        pOutInfo->format = kDiskImage_Amiga_DD_Floppy;
        pOutInfo->cylindersPerDisk = ADF_DD_CYLS_PER_DISK;
        pOutInfo->headsPerCylinder = ADF_DD_HEADS_PER_CYL;
        pOutInfo->sectorsPerTrack = ADF_DD_SECS_PER_TRACK;
        pOutInfo->bytesPerSector = ADF_SECTOR_DATA_SIZE;
        pOutInfo->physicalOffset = 0;
        pOutInfo->physicalSize = pOutInfo->cylindersPerDisk * pOutInfo->headsPerCylinder * pOutInfo->sectorsPerTrack * pOutInfo->bytesPerSector;
    }
    else {
        err = EINVAL;
    }

catch:
    if (fp) {
        fclose(fp);
    }
    return err;
}

// Converts a disk address to a LBA address. Note that cylinder and head
// parameters are 0-based while the sector parameter is 1-based.
// https://en.wikipedia.org/wiki/Cylinder-head-sector
errno_t di_lba_from_disk_addr(size_t* _Nonnull pOutLba, const DiskImage* _Nonnull info, const di_addr_t* _Nonnull addr)
{
    decl_try_err();

    switch (addr->type) {
        case di_addr_type_lba:
            *pOutLba = addr->u.lba;
            break;

        case di_addr_type_chs:
            if (addr->u.chs.cylinder >= info->cylindersPerDisk
                || addr->u.chs.head >= info->headsPerCylinder
                || addr->u.chs.sector == 0
                || addr->u.chs.sector > info->sectorsPerTrack) {
                err = ERANGE;
            }
            else {
                *pOutLba = (addr->u.chs.cylinder * info->headsPerCylinder + addr->u.chs.head) * info->sectorsPerTrack + (addr->u.chs.sector - 1);
            }
            break;

        default:
            abort();
            break;
    }

    return err;
}


//
// CLI parameter parsing
//

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

static errno_t parse_required_ulong(const char* str, char** str_end, size_t* pOutVal)
{
    if (!isdigit(*str)) {
        *str_end = (char*)str;
        return EINVAL;
    }

    errno = 0;
    *pOutVal = strtoul(str, str_end, 10);
    if (errno != 0) {
        return ERANGE;
    }

    return EOK;
}

static bool parseDiskAddress(di_addr_t* _Nonnull addr, const char* _Nonnull arg, di_slice_type sliceType)
{
    decl_try_err();
    char* p = (char*)arg;
    size_t v0, v1, v2;

    try(parse_required_ulong(p, &p, &v0));
    if (*p == '\0') {
        addr->type = di_addr_type_lba;
        addr->u.lba = v0;
        return true;
    }

    if (*p == ':') {
        try(parse_required_ulong(p + 1, &p, &v1));

        if (sliceType == di_slice_type_sector && *p == ':') {
            try(parse_required_ulong(p + 1, &p, &v2));
        }
        else {
            v2 = 1;
        }

        if (*p == '\0') {
            addr->type = di_addr_type_chs;
            addr->u.chs.cylinder = v0;
            addr->u.chs.head = v1;
            addr->u.chs.sector = v2;
            return true;
        }
    }

catch:
    return false;
}

static int parseSectorSlice(const char* _Nonnull proc_name, const struct clap_param_t* _Nonnull param, unsigned int eo, const char* _Nonnull arg)
{
    di_slice_t* slice = (di_slice_t*)param->value;

    slice->type = di_slice_type_sector;
    if (!parseDiskAddress(&slice->start, arg, di_slice_type_sector)) {
        clap_param_error(proc_name, param, eo, "invalid disk address '%s", arg);
        return EXIT_FAILURE;
    }
    else {
        return EXIT_SUCCESS;
    }
}

static int parseTrackSlice(const char* _Nonnull proc_name, const struct clap_param_t* _Nonnull param, unsigned int eo, const char* _Nonnull arg)
{
    di_slice_t* slice = (di_slice_t*)param->value;

    slice->type = di_slice_type_track;
    if (!parseDiskAddress(&slice->start, arg, di_slice_type_track)) {
        clap_param_error(proc_name, param, eo, "invalid disk address '%s", arg);
        return EXIT_FAILURE;
    }
    else {
        return EXIT_SUCCESS;
    }
}


clap_string_array_t paths = {NULL, 0};
const char* cmd_id = "";

// diskimage create
DiskImageFormat* disk_format = NULL;
size_t disk_size = 0;       // disk size as specified by user; 0 by default -> use the disk format defined size

// diskimage get
di_slice_t disk_slice = {0};
bool is_binary = false;

CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("diskimage <command> ..."),

    CLAP_REQUIRED_COMMAND("create", &cmd_id, "<root_path> <dimg_path>", "Creates an ADF disk image formatted with SerenaFS and which stores a copy of the files and directories rooted at 'root_path' in the local filesystem."),
        CLAP_VALUE('d', "disk", &disk_format, parseDiskFormat, "Specify the format of the generated disk image (default: Amiga DD)"),
        CLAP_VALUE('s', "size", &disk_size, parseDiskSize, "Set the size of the disk image (default: depends on the disk image format)"),
        CLAP_VARARG(&paths),

    CLAP_REQUIRED_COMMAND("describe", &cmd_id, "<dimg_path>", "Prints information about the disk image at path 'dimg_path'."),
        CLAP_VARARG(&paths),

    CLAP_REQUIRED_COMMAND("get", &cmd_id, "<dimg_path>", "Reads a sector from the ADF disk image 'dimg_path' and writes it to stdout."),
        CLAP_VALUE('s', "sector", &disk_slice, parseSectorSlice, "Specify a sector to read. Accepts a logical block address or a cylinder:head:sector style address"),
        CLAP_VALUE('t', "track", &disk_slice, parseTrackSlice, "Specify a track to read. Accepts a logical block address or a cylinder:head style address"),
        CLAP_BOOL('b', "binary", &is_binary, "Output the disk contents in binary"),
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
    decl_try_err();
    DiskImage info;

    gArgv_Zero = argv[0];
    clap_parse(0, params, argc, argv);

    init();

    if (!strcmp(argv[1], "create")) {
        // diskimage create
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

        try(cmd_createDiskImage(paths.strings[0], paths.strings[1], disk_format));
    }
    else if (!strcmp(argv[1], "describe")) {
        // diskimage describe
        if (paths.count != 1) {
            fatal("expected a disk image path");
            return EXIT_FAILURE;
        }

        try(di_describe_diskimage(paths.strings[0], &info));
        try(cmd_describeDiskImage(&info));
    }
    else if (!strcmp(argv[1], "get")) {
        // diskimage get
        if (paths.count != 1) {
            fatal("expected a disk image path");
            return EXIT_FAILURE;
        }

        switch (disk_slice.type) {
            case di_slice_type_sector:
            case di_slice_type_track:
                break;

            default:
                fatal("expected a disk address");
                return EXIT_FAILURE;
        }

        try(di_describe_diskimage(paths.strings[0], &info));
        try(cmd_getDiskSlice(paths.strings[0], &info, &disk_slice, !is_binary));
    }
    

    return EXIT_SUCCESS;

catch:
    fatal(strerror(err));
    return EXIT_FAILURE;
}
