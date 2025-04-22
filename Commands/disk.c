//
//  disk.c
//  cmds
//
//  Created by Dietmar Planitzer on 4/16/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <clap.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <System/Directory.h>
#include <System/Disk.h>
#include <System/Error.h>
#include <System/File.h>
#include <System/Filesystem.h>
#include <System/FilePermissions.h>
#include <System/Process.h>
#include <System/TimeInterval.h>
#include <System/Types.h>
#include <System/User.h>
#include <filesystem/serenafs/tools/format.h>

typedef struct di_permissions_spec {
    FilePermissions p;
    bool            isValid;
} di_permissions_spec_t;

typedef struct di_owner_spec {
    uid_t   uid;
    gid_t   gid;
    bool    isValid;
} di_owner_spec_t;



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


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: FSUtilities
////////////////////////////////////////////////////////////////////////////////

// Returns the current time. This time value is suitable for use as a timestamp
// for filesystem objects.
TimeInterval FSGetCurrentTime(void)
{
    // XXX consider switching to the clock API for more precision
    TimeInterval ti;

    ti.tv_sec = time(NULL);
    ti.tv_nsec = 0;

    return ti;
}

bool FSIsPowerOf2(size_t n)
{
    return (n && (n & (n - 1)) == 0) ? true : false;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Commands
////////////////////////////////////////////////////////////////////////////////

static errno_t block_write(intptr_t fd, const void* _Nonnull buf, LogicalBlockAddress blockAddr, size_t blockSize)
{
    if (fwrite(buf, blockSize, 1, (FILE*)fd) == 1) {
        return EOK;
    }
    else {
        return EIO;
    }
}

errno_t cmd_format(bool bQuick, FilePermissions rootDirPerms, uid_t rootDirUid, gid_t rootDirGid, const char* _Nonnull fsType, const char* _Nonnull label, const char* _Nonnull diskPath)
{
    decl_try_err();
    FILE* fp = NULL;
    DiskInfo info;

    if (strcmp(fsType, "sefs")) {
        throw(EINVAL);
    }

    try_null(fp, fopen(diskPath, "r+"), errno);
    setbuf(fp, NULL);

    try(IOChannel_Control(fileno(fp), kDiskCommand_GetInfo, &info));
    try(sefs_format((intptr_t)fp, block_write, info.blockCount, info.blockSize, rootDirUid, rootDirGid, rootDirPerms, label));
    puts("ok");

catch:
    if (fp) {
        fclose(fp);
    }
    return err;
}

errno_t cmd_fsid(const char* _Nonnull path)
{
    decl_try_err();
    FileInfo info;
    char* p = (char*)path;

    if (*p == '\0') {
        p = malloc(PATH_MAX);
        if (p == NULL) {
            return ENOMEM;
        }

        err = Process_GetWorkingDirectory(p, PATH_MAX);
    }


    if (err == EOK) {
        err = File_GetInfo(p, &info);
        if (err == EOK) {
            printf("%u\n", info.fsid);
        }
    }


    if (p != path) {
        free(p);
    }

    return err;
}

errno_t cmd_mount(const char* _Nonnull diskPath, const char* _Nonnull atPath)
{
    decl_try_err();

    err = Mount(kMount_Disk, diskPath, atPath, NULL, 0);

    return err;
}

errno_t cmd_unmount(const char* _Nonnull atPath, bool doForce)
{
    decl_try_err();
    UnmountOptions options = 0;

    if (doForce) {
        options |= kUnmount_Forced;
    }

    err = Unmount(atPath, options);

    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Command Line Parsing
////////////////////////////////////////////////////////////////////////////////

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
        owner->uid = (uid_t)uid;
        owner->gid = (gid_t)uid;
        owner->isValid = true;
        return EXIT_SUCCESS;
    }

    if (*p == ':') {
        try(parse_required_ulong(p + 1, &p, &gid));

        if (*p == '\0') {
            owner->uid = (uid_t)uid;
            owner->gid = (gid_t)gid;
            owner->isValid = true;
            return EXIT_SUCCESS;
        }
    }

catch:
    clap_param_error(proc_name, param, eo, "invalid ownership specification: '%s'", arg);
    return EXIT_FAILURE;
}


static const char* cmd_id = "";

// disk format
static bool should_quick_format = false;
static char* disk_path = "";
static char* fs_type = "";
static char* vol_label = "";
static di_permissions_spec_t permissions = {0, false};
static di_owner_spec_t owner = {kUserId_Root, kGroupId_Root, false};

// disk fsid
static char* fs_path = "";

// disk mount/unmount
static char* at_path = "";
static bool forced = false;


CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("disk <command> ..."),

    CLAP_REQUIRED_COMMAND("format", &cmd_id, "<disk_path>", "Formats the disk at 'disk_path' with the filesystem <fs_type> (SeFS)."),
        CLAP_BOOL('q', "quick", &should_quick_format, "Do a quick format"),
        CLAP_VALUE('m', "permissions", &permissions, parsePermissions, "Specify file/directory permissions as an octal number or a combination of 'rwx' characters"),
        CLAP_VALUE('o', "owner", &owner, parseOwnerId, "Specify the file/directory owner user and group id"),
        CLAP_STRING('l', "label", &vol_label, "Specify the volume label"),
        CLAP_STRING('t', "type", &fs_type, "Specify the filesystem type"),
        CLAP_POSITIONAL_STRING(&disk_path),

    CLAP_REQUIRED_COMMAND("fsid", &cmd_id, "<path>", "Prints the filesystem id of the filesystem at path 'path'."),
        CLAP_POSITIONAL_STRING(&fs_path),

    CLAP_REQUIRED_COMMAND("mount", &cmd_id, "<disk_path> --at <at_path>", "Mounts the disk 'disk_path' on top of the directory 'at_path'."),
        CLAP_STRING('t', "at", &at_path, "Specify the mount point"),
        CLAP_POSITIONAL_STRING(&disk_path),

    CLAP_REQUIRED_COMMAND("unmount", &cmd_id, "<at_path>", "Unmounts the filesystem at 'at_path'."),
        CLAP_BOOL('f', "force", &forced, "Force an unmount"),
        CLAP_POSITIONAL_STRING(&at_path)
);


int main(int argc, char* argv[])
{
    decl_try_err();

    clap_parse(0, params, argc, argv);
    
    if (!strcmp(cmd_id, "format")) {
        // disk format
        if (!permissions.isValid) {
            permissions.p = FilePermissions_MakeFromOctal(0777);
        }
        if (!owner.isValid) {
            owner.uid = kUserId_Root;
            owner.gid = kGroupId_Root;
            owner.isValid = true;
        }
        err = cmd_format(should_quick_format, permissions.p, owner.uid, owner.gid, fs_type, vol_label, disk_path);
    }
    else if (!strcmp(cmd_id, "fsid")) {
        // disk fsid
        err = cmd_fsid(fs_path);
    }
    else if (!strcmp(cmd_id, "mount")) {
        // disk mount
        err = cmd_mount(disk_path, at_path);
    }
    else if (!strcmp(cmd_id, "unmount")) {
        // disk unmount
        err = cmd_unmount(at_path, forced);
    }
    else {
        err = EINVAL;
    }


    if (err == EOK) {
        return EXIT_SUCCESS;
    }
    else {
        fatal(strerror(err));
        return EXIT_FAILURE;
    }
}
