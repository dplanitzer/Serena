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
#include <filemanager/FileHierarchy.h>
#include <filesystem/DirectoryChannel.h>
#include <filesystem/FileChannel.h>
#include <filesystem/SerenaDiskImage.h>
#include <filesystem/serenafs/SerenaFS.h>
#include <clap.h>
#include <System/ByteOrder.h>

static const char* gArgv_Zero = "";

void vfatal(const char* _Nonnull fmt, va_list ap)
{
    clap_verror(gArgv_Zero, fmt, ap);
    exit(EXIT_FAILURE);
    // NOT REACHED
}

void fatal(const char* _Nonnull fmt, ...)
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

// Converts a LBA-style disk address to a CHS-style disk address.
void di_chs_from_lba(size_t* _Nonnull pOutCylinder, size_t* _Nonnull pOutHead, size_t* _Nonnull pOutSector, const DiskImage* _Nonnull info, size_t lba)
{
    *pOutCylinder = lba / (info->headsPerCylinder * info->sectorsPerTrack);
    *pOutHead = (lba / info->sectorsPerTrack) % info->headsPerCylinder;
    *pOutSector = (lba % info->sectorsPerTrack) + 1;
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

static bool parseDiskFormat(const char* _Nonnull proc_name, const char* _Nonnull arg, DiskImageFormat* _Nonnull pOutFormat)
{
    long long size = 0ll;
    char* pptr = NULL;
    DiskImageFormat* de = &gDiskImageFormats[0];

    while (de->name) {
        if (!strcmp(arg, de->name)) {
            *pOutFormat = *de;
            return true;
        }
        de++;
    }

    clap_error(proc_name, "unknown disk image type '%s", arg);

    return false;
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

// c:h:s
static int parseSectorSlice(const char* _Nonnull proc_name, const struct clap_param_t* _Nonnull param, unsigned int eo, const char* _Nonnull arg)
{
    di_slice_t* slice = (di_slice_t*)param->value;

    slice->type = di_slice_type_sector;
    if (!parseDiskAddress(&slice->start, arg, di_slice_type_sector)) {
        clap_param_error(proc_name, param, eo, "invalid disk address '%s'", arg);
        return EXIT_FAILURE;
    }
    else {
        return EXIT_SUCCESS;
    }
}

// c:h
static int parseTrackSlice(const char* _Nonnull proc_name, const struct clap_param_t* _Nonnull param, unsigned int eo, const char* _Nonnull arg)
{
    di_slice_t* slice = (di_slice_t*)param->value;

    slice->type = di_slice_type_track;
    if (!parseDiskAddress(&slice->start, arg, di_slice_type_track)) {
        clap_param_error(proc_name, param, eo, "invalid disk address '%s'", arg);
        return EXIT_FAILURE;
    }
    else {
        return EXIT_SUCCESS;
    }
}

// -p=rwxrwxrwx | -p=777
static int parsePermissions(const char* _Nonnull proc_name, const struct clap_param_t* _Nonnull param, unsigned int eo, const char* _Nonnull arg)
{
    di_permissions_spec_t* out_perms = (di_permissions_spec_t*)param->value;

    if (isdigit(*arg)) {
        char* ep;
        unsigned long bits = strtoul(arg, &ep, 8);

        if (*ep != '\0' || bits == 0 || (bits == ULONG_MAX && errno != 0)) {
            clap_param_error(proc_name, param, eo, "invalid permissions: '%s'", arg);
            return EXIT_FAILURE;
        }

        out_perms->p = FilePermissions_MakeFromOctal(bits & 0777);
        out_perms->isValid = true;
    }
    else if (*arg != '\0') {
        FilePermissions perms[3] = {0, 0, 0};
        const char* str = arg;

        for (int i = 0; i < 3; i++) {
            FilePermissions t = 0;

            for (int j = 0; j < 3; j++) {
                switch (*str++) {
                    case 'r': t |= kFilePermission_Read; break;
                    case 'w': t |= kFilePermission_Write; break;
                    case 'x': t |= kFilePermission_Execute; break;
                    case '-': break;
                    case '_': break;
                    default:
                        clap_param_error(proc_name, param, eo, "invalid permissions: '%s'", arg);
                        return EXIT_FAILURE;
                }
            }

            perms[i] = t;
        }

        if (*str != '\0') {
            clap_param_error(proc_name, param, eo, "invalid permissions: '%s'", arg);
            return EXIT_FAILURE;
        }

        out_perms->p = FilePermissions_Make(perms[0], perms[1], perms[2]);
        out_perms->isValid = true;
    }
    else {
        clap_param_error(proc_name, param, eo, "expected permissions");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

// -o=uid:gid
static int parseOwnerId(const char* _Nonnull proc_name, const struct clap_param_t* _Nonnull param, unsigned int eo, const char* _Nonnull arg)
{
    di_owner_spec_t* owner = (di_owner_spec_t*)param->value;

    decl_try_err();
    char* p = (char*)arg;
    size_t uid, gid;

    try(parse_required_ulong(p, &p, &uid));
    if (*p == '\0') {
        owner->u.uid = (UserId)uid;
        owner->isValid = true;
        return EXIT_SUCCESS;
    }

    if (*p == ':') {
        try(parse_required_ulong(p + 1, &p, &gid));

        if (*p == '\0') {
            owner->u.uid = (UserId)uid;
            owner->u.gid = (GroupId)gid;
            owner->isValid = true;
            return EXIT_SUCCESS;
        }
    }

catch:
    clap_param_error(proc_name, param, eo, "invalid ownership specification: '%s'", arg);
    return EXIT_FAILURE;
}


static void assert_has_slice_type(const di_slice_t* _Nonnull slice)
{
    switch (slice->type) {
        case di_slice_type_sector:
        case di_slice_type_track:
            break;

        default:
            fatal("expected a disk address");
            break;
    }
}


static di_permissions_spec_t permissions = {0, false};
static di_owner_spec_t owner = {{0, 0}, false};
static clap_string_array_t paths = {NULL, 0};
static const char* cmd_id = "";

// diskimage create
static size_t disk_size = 0;       // disk size as specified by user; 0 by default -> use the disk format defined size
static char* disk_type = "";
static char* dmg_path = "";

// diskimage diff
static char* dmg_path2 = "";

// diskimage get
static di_slice_t disk_slice = {0};
static bool is_hex = false;

// diskimage delete
static char* path = "";

// diskimage format
static bool should_quick_format = false;
static char* fs_type = "";

// diskimage makedir
static bool should_create_parents = false;

// diskimage pull
static char* dst_path = "";

// diskimage push
static char* src_path = "";


CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("diskimage <command> ..."),

    //
    // block-level commands
    //

    CLAP_REQUIRED_COMMAND("create", &cmd_id, "<disk_type> <dimg_path>", "Creates an empty disk image file of format 'disk_type' and stores it in the location 'dimg_path'."),
        CLAP_VALUE('s', "size", &disk_size, parseDiskSize, "Set the size of the disk image (default: depends on the disk image format)"),
        CLAP_POSITIONAL_STRING(&disk_type),
        CLAP_POSITIONAL_STRING(&dmg_path),

    CLAP_REQUIRED_COMMAND("describe", &cmd_id, "<dimg_path>", "Prints information about the disk image at path 'dimg_path'."),
        CLAP_POSITIONAL_STRING(&dmg_path),

    CLAP_REQUIRED_COMMAND("diff", &cmd_id, "<dimg1_path> <dimg2_path>", "Compares disk images 'dimg1_path' and 'dimg2_path' and prints a list of the sectors with differing contents."),
        CLAP_POSITIONAL_STRING(&dmg_path),
        CLAP_POSITIONAL_STRING(&dmg_path2),

    CLAP_REQUIRED_COMMAND("get", &cmd_id, "<dimg_path>", "Reads a sector from the ADF disk image 'dimg_path' and writes it to stdout."),
        CLAP_VALUE('s', "sector", &disk_slice, parseSectorSlice, "Specify a sector to read. Accepts a logical block address or a cylinder:head:sector style address"),
        CLAP_VALUE('t', "track", &disk_slice, parseTrackSlice, "Specify a track to read. Accepts a logical block address or a cylinder:head style address"),
        CLAP_BOOL('x', "hex", &is_hex, "Output the disk contents as a hex dump instead of binary data"),
        CLAP_POSITIONAL_STRING(&dmg_path),

    CLAP_REQUIRED_COMMAND("put", &cmd_id, "<dimg_path>", "Replaces a sector in the ADF disk image 'dimg_path' with bytes from stdin."),
        CLAP_VALUE('s', "sector", &disk_slice, parseSectorSlice, "Specify a sector to replace. Accepts a logical block address or a cylinder:head:sector style address"),
        CLAP_VALUE('t', "track", &disk_slice, parseTrackSlice, "Specify a track to replace. Accepts a logical block address or a cylinder:head style address"),
        CLAP_POSITIONAL_STRING(&dmg_path),


    //
    // filesystem-level commands
    //

    CLAP_REQUIRED_COMMAND("delete", &cmd_id, "<path> <dimg_path>", "Deletes the file or directory at 'path' in the disk image 'dimg_path'."),
        CLAP_POSITIONAL_STRING(&path),
        CLAP_POSITIONAL_STRING(&dmg_path),

    CLAP_REQUIRED_COMMAND("format", &cmd_id, "<fs_type> <dimg_path>", "Formats the disk image 'dimg_path' with teh filesystem <fs_type> (SeFS)."),
        CLAP_BOOL('q', "quick", &should_quick_format, "Do a quick format"),
        CLAP_VALUE('m', "permissions", &permissions, parsePermissions, "Specify file/directory permissions as an octal number or a combination of 'rwx' characters"),
        CLAP_VALUE('o', "owner", &owner, parseOwnerId, "Specify the file/directory owner user and group id"),
        CLAP_POSITIONAL_STRING(&fs_type),
        CLAP_POSITIONAL_STRING(&dmg_path),

    CLAP_REQUIRED_COMMAND("list", &cmd_id, "<path> <dimg_path>", "Lists the contents of the directory 'path' in the disk image 'dimg_path'."),
        CLAP_POSITIONAL_STRING(&path),
        CLAP_POSITIONAL_STRING(&dmg_path),

    CLAP_REQUIRED_COMMAND("makedir", &cmd_id, "<path> <dimg_path>", "Creates a new directory at 'path' in the disk image 'dimg_path'."),
        CLAP_BOOL('p', "parents", &should_create_parents, "Create missing parent directories"),
        CLAP_VALUE('m', "permissions", &permissions, parsePermissions, "Specify file/directory permissions as an octal number or a combination of 'rwx' characters"),
        CLAP_VALUE('o', "owner", &owner, parseOwnerId, "Specify the file/directory owner user and group id"),
        CLAP_POSITIONAL_STRING(&path),
        CLAP_POSITIONAL_STRING(&dmg_path),

    CLAP_REQUIRED_COMMAND("pull", &cmd_id, "<path> <dst_path> <dimg_path>", "Copies the file at 'path' in the disk image 'dimg_path' to the location 'dst_path' in the local filesystem."),
        CLAP_POSITIONAL_STRING(&path),
        CLAP_POSITIONAL_STRING(&dst_path),
        CLAP_POSITIONAL_STRING(&dmg_path),

    CLAP_REQUIRED_COMMAND("push", &cmd_id, "<src_path> <path> <dimg_path>", "Copies the file at 'src_path' stored in the local filesystem to the location 'path' in the disk image 'dimg_path'."),
        CLAP_VALUE('m', "permissions", &permissions, parsePermissions, "Specify file/directory permissions as an octal number or a combination of 'rwx' characters"),
        CLAP_VALUE('o', "owner", &owner, parseOwnerId, "Specify the file/directory owner user and group id"),
        CLAP_POSITIONAL_STRING(&src_path),
        CLAP_POSITIONAL_STRING(&path),
        CLAP_POSITIONAL_STRING(&dmg_path)
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
    _RegisterClass(&kIOChannelClass);
    _RegisterClass(&kFileChannelClass);
    _RegisterClass(&kDirectoryChannelClass);
    _RegisterClass(&kFileHierarchyClass);
}

int main(int argc, char* argv[])
{
    decl_try_err();

    gArgv_Zero = argv[0];
    clap_parse(0, params, argc, argv);

    init();

    if (!strcmp(argv[1], "create")) {
        // diskimage create
        DiskImageFormat fmt;
        if (!parseDiskFormat(gArgv_Zero, disk_type, &fmt)) {
            return EXIT_FAILURE;
        }
        
        if (disk_size > 0 && fmt.format == kDiskImage_Serena) {
            fmt.blocksPerDisk = disk_size / fmt.blockSize;
            if (fmt.blocksPerDisk * fmt.blockSize < disk_size) {
                fmt.blocksPerDisk++;
            }
        }

        try(cmd_create(&fmt, dmg_path));
    }
    else if (!strcmp(argv[1], "describe")) {
        // diskimage describe
        try(cmd_describe_disk(dmg_path));
    }
    else if (!strcmp(argv[1], "diff")) {
        // diskimage get
        try(cmd_diff_disks(dmg_path, dmg_path2));
    }
    else if (!strcmp(argv[1], "get")) {
        // diskimage get
        assert_has_slice_type(&disk_slice);

        try(cmd_get_disk_slice(dmg_path, &disk_slice, is_hex));
    }
    else if (!strcmp(argv[1], "put")) {
        // diskimage put
        assert_has_slice_type(&disk_slice);

        try(cmd_put_disk_slice(dmg_path, &disk_slice));
    }
    else if (!strcmp(argv[1], "delete")) {
        // diskimage delete
        try(cmd_delete(path, dmg_path));
    }
    else if (!strcmp(argv[1], "format")) {
        // diskimage format
        if (!permissions.isValid) {
            permissions.p = FilePermissions_MakeFromOctal(0755);
        }
        if (!owner.isValid) {
            owner.u = kUser_Root;
        }
        try(cmd_format(should_quick_format, permissions.p, owner.u, fs_type, dmg_path));
    }
    else if (!strcmp(argv[1], "list")) {
        // diskimage list
        try(cmd_list(path, dmg_path));
    }
    else if (!strcmp(argv[1], "makedir")) {
        // diskimage makedir
        if (!permissions.isValid) {
            permissions.p = FilePermissions_MakeFromOctal(0755);
        }
        if (!owner.isValid) {
            owner.u = kUser_Root;
        }
        try(cmd_makedir(should_create_parents, permissions.p, owner.u, path, dmg_path));
    }
    else if (!strcmp(argv[1], "pull")) {
        // diskimage pull
        try(cmd_pull(path, dst_path, dmg_path));
    }
    else if (!strcmp(argv[1], "push")) {
        // diskimage push
        if (!permissions.isValid) {
            permissions.p = FilePermissions_MakeFromOctal(0644);
        }
        if (!owner.isValid) {
            owner.u = kUser_Root;
        }
        try(cmd_push(permissions.p, owner.u, src_path, path, dmg_path));
    }


    return EXIT_SUCCESS;

catch:
    fatal(strerror(err));
    return EXIT_FAILURE;
}
