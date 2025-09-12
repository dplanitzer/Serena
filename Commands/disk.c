//
//  disk.c
//  cmds
//
//  Created by Dietmar Planitzer on 4/16/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <clap.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/disk.h>
#include <sys/fs.h>
#include <sys/mount.h>
#include <sys/fs.h>
#include <sys/ioctl.h>
#include <sys/perm.h>
#include <sys/stat.h>
#include <sys/uid.h>
#include <filesystem/serenafs/tools/format.h>

typedef struct di_permissions_spec {
    mode_t  p;
    bool    isValid;
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
// MARK: FSUtilities
////////////////////////////////////////////////////////////////////////////////

// Returns the current time. This time value is suitable for use as a timestamp
// for filesystem objects.
void FSGetCurrentTime(struct timespec* _Nonnull ts)
{
    // XXX consider switching to the clock API for more precision
    ts->tv_sec = time(NULL);
    ts->tv_nsec = 0;
}

bool FSIsPowerOf2(size_t n)
{
    return (n && (n & (n - 1)) == 0) ? true : false;
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
    lseek(ioc, 0ll, SEEK_SET);
    while (t < trackCount) {
        printf("Formatting track: %u of %u\r", (unsigned)(t + 1), (unsigned)trackCount);
        
        if (ioctl(ioc, kDiskCommand_FormatTrack, NULL, 0) != 0) {
            ok = 0;
            break;
        }
        
        t++;
    }
    puts("\033[?25h");

    return ok;
}

void cmd_format(bool bQuick, mode_t rootDirPerms, uid_t rootDirUid, gid_t rootDirGid, const char* _Nonnull fsType, const char* _Nonnull label, const char* _Nonnull diskPath)
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
                    lseek(fd, 0ll, SEEK_SET);
                }
            }
            if (ok) {
                puts("Initializing filesystem...");
                const errno_t err = sefs_format((intptr_t)fp, block_write, info.sectorsPerTrack * info.heads * info.cylinders, info.sectorSize, rootDirUid, rootDirGid, rootDirPerms, label);
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
    struct stat info;
    char* p = (char*)path;

    if (*p == '\0') {
        p = path_buf;
        getcwd(p, PATH_MAX);
    }


    if (stat(p, &info) == 0) {
        return info.st_fsid;
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


static void print_cat_info(const fs_info_t* _Nonnull info, int fd)
{
    char diskName[32];

    fs_getdisk(info->fsid, diskName, sizeof(diskName));

    puts("Catalog ID");
    printf("%s       %u\n", diskName, info->fsid);
}

static void print_reg_info(const fs_info_t* _Nonnull info, int fd)
{
    const uint64_t size = (uint64_t)info->capacity * (uint64_t)info->blockSize;
    const unsigned fullPerc = info->count * 100 / info->capacity;   // XXX round up to the next %
    const char* status;
    char diskName[32];
    char volLabel[64];

    fs_getdisk(info->fsid, diskName, sizeof(diskName));
    if (ioctl(fd, kFSCommand_GetLabel, volLabel, sizeof(volLabel)) != 0) {
        return;
    }


    if ((info->properties & kFSProperty_IsReadOnly) == kFSProperty_IsReadOnly) {
        status = "Read Only";
    }
    else {
        status = "Read/Write";
    }


    // XX formatting
    puts("Disk ID Size   Used   Free Full Status Type Name");
    printf("%s %u %lluK %u %u %u%% %s %s %s\n", diskName, info->fsid, size / 1024ull, info->count, info->capacity - info->count, fullPerc, status, info->type, volLabel);
}

static int open_fs(const char* _Nonnull path, fsid_t* _Nullable pfsid)
{
    const fsid_t fsid = get_fsid(path);
    char buf[32];

    if (fsid != -1) {
        sprintf(buf, "/fs/%u", fsid);
        
        const int fd = open(buf, O_RDONLY);
        if (fd >= 0) {
            if (pfsid) *pfsid = fsid;
            return fd;
        }
    }

    return -1;
}

static void cmd_info(const char* _Nonnull path)
{
    const int fd = open_fs(path, NULL);
    fs_info_t info;

    if (ioctl(fd, kFSCommand_GetInfo, &info) == 0) {
        if ((info.properties & kFSProperty_IsCatalog) == kFSProperty_IsCatalog) {
            print_cat_info(&info, fd);
        }
        else {
            print_reg_info(&info, fd);
        }
    }
    close(fd);
}


static void cmd_geometry(const char* _Nonnull path)
{
    struct stat st;
    fsid_t fsid;
    fs_info_t fsinf;
    char buf[32];
    disk_info_t di;
    int fd = -1;
    bool hasDisk = true;

    if (*path != '\0') {
        if (stat(path, &st) != 0) {
            return;
        }
    }

    if (S_ISDEV(st.st_mode)) {
        fd = open(path, O_RDONLY);
        if (fd < 0) {
            return;
        }
        ioctl(fd, kDiskCommand_GetDiskInfo, &di);
    }
    else {
        if ((fd = open_fs(path, &fsid)) >= 0) {
            ioctl(fd, kFSCommand_GetDiskInfo, &di);
        }

        fs_getdisk(fsid, buf, sizeof(buf));
        path = buf;
    }
    close(fd);


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
        close(fd);
    }
}

void cmd_mount(const char* _Nonnull diskPath, const char* _Nonnull atPath)
{
    sense_disk(diskPath);
    mount(kMount_SeFS, diskPath, atPath, "");
}


void cmd_unmount(const char* _Nonnull atPath, bool doForce)
{
    UnmountOptions options = 0;

    if (doForce) {
        options |= kUnmount_Forced;
    }

    unmount(atPath, options);
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

        out_perms->p = perm_from_octal(bits & 0777);
        out_perms->isValid = true;
    }
    else if (*arg != '\0') {
        mode_t perms[3] = {0, 0, 0};
        const char* str = arg;

        for (int i = 0; i < 3; i++) {
            mode_t t = 0;

            for (int j = 0; j < 3; j++) {
                switch (*str++) {
                    case 'r': t |= S_IREAD; break;
                    case 'w': t |= S_IWRITE; break;
                    case 'x': t |= S_IEXEC; break;
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

        out_perms->p = perm_from(perms[0], perms[1], perms[2]);
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
            permissions.p = perm_from_octal(0777);
        }
        if (!owner.isValid) {
            owner.uid = kUserId_Root;
            owner.gid = kGroupId_Root;
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
        fatal(strerror(errno));
        return EXIT_FAILURE;
    }
}
