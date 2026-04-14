//
//  disk.c
//  cmd/disk
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
#include <serena/directory.h>
#include <serena/file.h>
#include <serena/disk.h>
#include <serena/filesystem.h>
#include <serena/ioctl.h>
#include <serena/process.h>
#include <serena/uid.h>
#include "sefs_init.h"

typedef struct di_permissions_spec {
    fs_perms_t  p;
    bool        isValid;
} di_permissions_spec_t;

typedef struct di_owner_spec {
    uid_t   uid;
    gid_t   gid;
    bool    isValid;
} di_owner_spec_t;



static char path_buf[PATH_MAX];
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
// MARK: Commands
////////////////////////////////////////////////////////////////////////////////

static errno_t block_write(intptr_t fd, const void* _Nonnull buf, blkno_t blockAddr, size_t blockSize)
{
    if (fwrite(buf, blockSize, 1, (FILE*)fd) == 1) {
        return EOK;
    }
    else {
        return errno;
    }
}

static int wipe_disk(int ioc, const disk_info_t* _Nonnull ip)
{
    size_t t = 0, trackCount = ip->cylinders * ip->heads;
    int ok = 1;
    
    fputs("\033[?25l", stdout);
    // Try format_disk() first
    if (ioctl(ioc, kDiskCommand_FormatDisk, 0) != 0) {
        if (errno == ENOTSUP) {
            errno = 0;
            
            // Try formatting individual tracks next
            fd_seek(ioc, 0ll, SEEK_SET);
            while (t < trackCount) {
                printf("Formatting track: %u of %u\r", (unsigned)(t + 1), (unsigned)trackCount);
                fflush(stdout);
        
                if (ioctl(ioc, kDiskCommand_FormatTrack, 0) != 0) {
                    ok = 0;
                    break;
                }
        
                t++;
            }
        }
        else {
            ok = 0;
        }
    }
    puts("\033[?25h");

    return ok;
}

void cmd_format(bool bQuick, fs_perms_t rootDirPerms, uid_t rootDirUid, gid_t rootDirGid, const char* _Nonnull fsType, const char* _Nonnull label, const char* _Nonnull diskPath)
{
    FILE* fp = NULL;
    disk_info_t info;

    if (strcmp(fsType, "sefs")) {
        errno = EINVAL;
        return;
    }

    if ((fp = fopen(diskPath, "r+")) != NULL) {
        const int fd = fileno(fp);
        int ok = 1;

        setbuf(fp, NULL);

        if (ioctl(fd, kDiskCommand_SenseDisk) == 0) {
            ioctl(fd, kDiskCommand_GetDiskInfo, &info);
            if (!bQuick) {
                ok = wipe_disk(fd, &info);
                if (ok) {
                    fd_seek(fd, 0ll, SEEK_SET);
                }
            }
            if (ok) {
                struct timespec now;

                // XXX consider switching to the clock API for more precision
                now.tv_sec = time(NULL);
                now.tv_nsec = 0;

                puts("Initializing filesystem...");
                const errno_t err = sefs_init((intptr_t)fp, block_write, info.sectorsPerTrack * info.heads * info.cylinders, info.sectorSize, &now, rootDirUid, rootDirGid, rootDirPerms, label);
                if (err != EOK) {
                    ok = 0;
                    errno = err;
                }
            }
            if (ok) {
                puts("Done");
            }
        }
        
        fclose(fp);
    }
}


// Returns the fsid of the current working directory if 'path' is an empty string
// and of the filesystem that owns path otherwise.
static fsid_t get_fsid(const char* _Nonnull path)
{
    fs_attr_t attr;
    char* p = (char*)path;

    if (*p == '\0') {
        p = path_buf;
        proc_cwd(p, PATH_MAX);
    }


    if (fs_attr(NULL, p, &attr) == 0) {
        return attr.fsid;
    }
    else {
        return -1;
    }
}

void cmd_fsid(const char* _Nonnull path)
{
    const fsid_t fsid = get_fsid(path);

    if (fsid != -1) {
        printf("%u\n", fsid);
    }
}


static void print_reg_info(const fs_basic_info_t* _Nonnull info)
{
    const uint64_t size = (uint64_t)info->capacity * (uint64_t)info->blockSize;
    const unsigned fullPerc = info->count * 100 / info->capacity;   // XXX round up to the next %
    const char* status;
    char diskName[32];
    char volLabel[64];

    if (fs_property(info->fsid, FS_PROP_DISKPATH, diskName, sizeof(diskName)) == -1) {
        return;
    }
    if (fs_property(info->fsid, FS_PROP_LABEL, volLabel, sizeof(volLabel)) == -1) {
        return;
    }


    if ((info->flags & FS_FLAG_READ_ONLY) == FS_FLAG_READ_ONLY) {
        status = "Read Only";
    }
    else {
        status = "Read/Write";
    }


    // XX formatting
    puts("Disk ID Size   Used   Free Full Status Type Name");
    printf("%s %u %lluK %u %u %u%% %s %s %s\n", diskName, info->fsid, size / 1024ull, info->count, info->capacity - info->count, fullPerc, status, info->type, volLabel);
}

static void print_cat_info(const fs_basic_info_t* _Nonnull info)
{
    char diskName[32];

    if (fs_property(info->fsid, FS_PROP_DISKPATH, diskName, sizeof(diskName)) == -1) {
        return;
    }

    puts("Catalog ID");
    printf("%s       %u\n", diskName, info->fsid);
}

static void cmd_info(const char* _Nonnull path)
{
    const fsid_t fsid = get_fsid(path);
    fs_basic_info_t info;

    if (!fs_info(fsid, FS_INFO_BASIC, &info)) {
        if ((info.flags & FS_FLAG_CATALOG) == FS_FLAG_CATALOG) {
            print_cat_info(&info);
        }
        else {
            print_reg_info(&info);
        }
    }
}


static void cmd_geometry(const char* _Nonnull path)
{
    fs_attr_t attr;
    fsid_t fsid;
    char buf[32];
    disk_info_t di;
    int fd = -1;
    bool hasDisk = true;

    if (*path != '\0') {
        if (fs_attr(NULL, path, &attr) != 0) {
            return;
        }
    }

    if (attr.file_type == FS_FTYPE_DEV) {
        // raw disk
        fd = open(path, O_RDONLY);
        if (fd < 0) {
            return;
        }
        ioctl(fd, kDiskCommand_GetDiskInfo, &di);
        fd_close(fd);
    }
    else {
        // filesystem
        const fsid_t fsid = get_fsid(path);

        if (fs_info(fsid, FS_INFO_DISK, &di)) {
            return;
        }

        fs_property(fsid, FS_PROP_DISKPATH, buf, sizeof(buf));
        path = buf;
    }


    if (errno == ENOMEDIUM) {
        errno = EOK;
        hasDisk = false;
    }
    else if (errno != 0) {
        return;
    }


    // XX formatting
    if (hasDisk) {
        puts("Disk Cylinders Heads Sectors/Track Sectors/Disk Sector Size");
        printf("%s  %zu  %zu  %zu  %zu  %zu\n", path, di.cylinders, di.heads, di.sectorsPerTrack, di.sectorsPerDisk, di.sectorSize);
    }
    else {
        puts("Disk");
        printf("%s  no disk in drive\n", path);
    }
}


static void sense_disk(const char* _Nonnull diskPath)
{
    const int fd = open(diskPath, O_RDONLY);

    if (fd >= 0) {
        ioctl(fd, kDiskCommand_SenseDisk);
        fd_close(fd);
    }
}

void cmd_mount(const char* _Nonnull diskPath, const char* _Nonnull atPath)
{
    sense_disk(diskPath);
    fs_mount(FS_MOUNT_SEFS, diskPath, atPath, "");
}


void cmd_unmount(const char* _Nonnull atPath, bool doForce)
{
    int flags = 0;

    if (doForce) {
        flags |= FS_UNMOUNT_FORCE;
    }

    fs_unmount(atPath, flags);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Command Line Parsing
////////////////////////////////////////////////////////////////////////////////

static void parse_required_ulong(const char* str, char** str_end, size_t* pOutVal)
{
    if (!isdigit(*str)) {
        *str_end = (char*)str;
        errno = EINVAL;
    }
    else {
        *pOutVal = strtoul(str, str_end, 10);
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

        out_perms->p = fs_perms_from_octal(bits & 0777);
        out_perms->isValid = true;
    }
    else if (*arg != '\0') {
        fs_perms_t perms[3] = {0, 0, 0};
        const char* str = arg;

        for (int i = 0; i < 3; i++) {
            fs_perms_t t = 0;

            for (int j = 0; j < 3; j++) {
                switch (*str++) {
                    case 'r': t |= FS_PRM_R; break;
                    case 'w': t |= FS_PRM_W; break;
                    case 'x': t |= FS_PRM_X; break;
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

        out_perms->p = fs_perms_from(perms[0], perms[1], perms[2]);
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

    char* p = (char*)arg;
    size_t uid, gid;

    errno = 0;
    parse_required_ulong(p, &p, &uid);
    if (errno == 0) {
        if (*p == '\0') {
            owner->uid = (uid_t)uid;
            owner->gid = (gid_t)uid;
            owner->isValid = true;
            return EXIT_SUCCESS;
        }

        if (*p == ':') {
            parse_required_ulong(p + 1, &p, &gid);

            if (errno == 0 && *p == '\0') {
                owner->uid = (uid_t)uid;
                owner->gid = (gid_t)gid;
                owner->isValid = true;
                return EXIT_SUCCESS;
            }
        }
    }

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
static di_owner_spec_t owner = {UID_ROOT, GID_ROOT, false};

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

    CLAP_REQUIRED_COMMAND("geometry", &cmd_id, "<disk_path>", "Prints information about the geometry of the disk device at path 'disk_path'."),
        CLAP_POSITIONAL_STRING(&disk_path),

    CLAP_REQUIRED_COMMAND("mount", &cmd_id, "<disk_path> --at <at_path>", "Mounts the disk 'disk_path' on top of the directory 'at_path'."),
        CLAP_STRING('t', "at", &at_path, "Specify the mount point"),
        CLAP_POSITIONAL_STRING(&disk_path),

    CLAP_REQUIRED_COMMAND("unmount", &cmd_id, "<at_path>", "Unmounts the filesystem at 'at_path'."),
        CLAP_BOOL('f', "force", &forced, "Force an unmount"),
        CLAP_POSITIONAL_STRING(&at_path)
);


int main(int argc, char* argv[])
{
    clap_parse(0, params, argc, argv);
    
    if (!strcmp(cmd_id, "format")) {
        // disk format
        if (!permissions.isValid) {
            permissions.p = fs_perms_from_octal(0777);
        }
        if (!owner.isValid) {
            owner.uid = UID_ROOT;
            owner.gid = GID_ROOT;
            owner.isValid = true;
        }
        cmd_format(should_quick_format, permissions.p, owner.uid, owner.gid, fs_type, vol_label, disk_path);
    }
    else if (!strcmp(cmd_id, "fsid")) {
        // disk fsid
        cmd_fsid(fs_path);
    }
    else if (!strcmp(cmd_id, "info")) {
        // disk info
        cmd_info(fs_path);
    }
    else if (!strcmp(cmd_id, "geometry")) {
        // disk geometry
        cmd_geometry(disk_path);
    }
    else if (!strcmp(cmd_id, "mount")) {
        // disk mount
        cmd_mount(disk_path, at_path);
    }
    else if (!strcmp(cmd_id, "unmount")) {
        // disk unmount
        cmd_unmount(at_path, forced);
    }
    else {
        errno = EINVAL;
    }


    if (errno == EOK) {
        return EXIT_SUCCESS;
    }
    else {
        fatal("%s", strerror(errno));
        return EXIT_FAILURE;
    }
}
