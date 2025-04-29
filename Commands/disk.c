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

static errno_t wipe_disk(int ioc, const DiskInfo* _Nonnull info)
{
    decl_try_err();
    FormatSectorsRequest req;

    req.mediaId = info->mediaId;
    req.addr = 0;
    req.data = malloc(info->sectorSize * info->frClusterSize);
    if (req.data == NULL) {
        return ENOMEM;
    }

    
    for (LogicalBlockCount i = 0; i < info->frClusterSize; i++) {
        memset(&((uint8_t*)req.data)[i * info->sectorSize], i + 1, info->sectorSize);
    }

    fputs("\033[?25l", stdout);
    while (req.addr < info->sectorCount && err == EOK) {
        printf("%u\n\033[1A", (unsigned)req.addr);
        err = IOChannel_Control(ioc, kDiskCommand_Format, &req);
        req.addr += info->frClusterSize;
    }
    fputs("\033[?25h\n", stdout);

    free(req.data);

    return err;
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
    if (!bQuick) {
        try(wipe_disk(fileno(fp), &info));
    }
    try(sefs_format((intptr_t)fp, block_write, info.sectorCount, info.sectorSize, rootDirUid, rootDirGid, rootDirPerms, label));
    puts("ok");

catch:
    if (fp) {
        fclose(fp);
    }
    return err;
}


// Returns the fsid of teh current working directory if 'path' is an empty string
// and of the filesystem that owns path otherwise.
static errno_t get_fsid(const char* _Nonnull path, fsid_t* _Nonnull fsid)
{
    decl_try_err();
    FileInfo info;
    char* p = (char*)path;

    *fsid = 0;

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
            *fsid = info.fsid;
        }
    }


    if (p != path) {
        free(p);
    }

    return err;
}

errno_t cmd_fsid(const char* _Nonnull path)
{
    decl_try_err();
    fsid_t fsid;

    err = get_fsid(path, &fsid);
    if (err == EOK) {
        printf("%u\n", fsid);
    }

    return err;
}


static errno_t print_cat_info(const FSInfo* _Nonnull info, int fd)
{
    decl_try_err();
    char diskName[32];

    try(s_fsgetdisk(info->fsid, diskName, sizeof(diskName)));

    puts("Catalog ID");
    printf("%s       %u\n", diskName, info->fsid);

catch:
    return err;
}

static errno_t print_reg_info(const FSInfo* _Nonnull info, int fd)
{
    decl_try_err();
    const uint64_t size = (uint64_t)info->capacity * (uint64_t)info->blockSize;
    const unsigned fullPerc = info->count * 100 / info->capacity;   // XXX round up to the next %
    const char* status;
    char diskName[32];
    char volLabel[64];

    try(s_fsgetdisk(info->fsid, diskName, sizeof(diskName)));
    try(IOChannel_Control(fd, kFSCommand_GetLabel, volLabel, sizeof(volLabel)));


    if ((info->properties & kFSProperty_IsReadOnly) == kFSProperty_IsReadOnly) {
        status = "Read Only";
    }
    else {
        status = "Read/Write";
    }


    // XX formatting
    puts("Disk ID Size   Used   Free Full Status Type Name");
    printf("%s %u %lluK %u %u %u%% %s %s %s\n", diskName, info->fsid, size / 1024ull, info->count, info->capacity - info->count, fullPerc, status, info->type, volLabel);

catch:
    return err;
}

static errno_t cmd_info(const char* _Nonnull path)
{
    decl_try_err();
    fsid_t fsid;
    FSInfo info;
    int fd = -1;
    char buf[32];

    try(get_fsid(path, &fsid));
    sprintf(buf, "/fs/%u", fsid);
    try(File_Open(buf, kOpen_Read, &fd));
    try(IOChannel_Control(fd, kFSCommand_GetInfo, &info));

    if ((info.properties & kFSProperty_IsCatalog) == kFSProperty_IsCatalog) {
        err = print_cat_info(&info, fd);
    }
    else {
        err = print_reg_info(&info, fd);
    }


catch:
    if (fd >= 0) {
        IOChannel_Close(fd);
    }

    return err;
}


errno_t cmd_mount(const char* _Nonnull diskPath, const char* _Nonnull atPath)
{
    decl_try_err();

    err = Mount(kMount_Disk, diskPath, atPath, "");

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

    CLAP_REQUIRED_COMMAND("info", &cmd_id, "<path>", "Prints information about the filesystem at path 'path'."),
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
    else if (!strcmp(cmd_id, "info")) {
        // disk info
        err = cmd_info(fs_path);
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
