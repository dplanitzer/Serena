//
//  list.c
//  cmds
//
//  Created by Dietmar Planitzer on 4/22/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <clap.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <ext/math.h>
#include <ext/stdlib.h>
#include <ext/string.h>
#include <serena/directory.h>
#include <serena/file.h>
#include <serena/process.h>

extern const char* __gc_abbrev_ymon(unsigned m);


// Jan 12  2025
#define DATE_WIDTH (3 + 1 + 2 + 1 + 4)

// Jan 12 13:45
#define TIME_WIDTH (3 + 1 + 2 + 1 + 5)

// Buffer used for various conversions
#define BUF_SIZE    32

// Max length of a permission string
#define PERMISSIONS_STRING_LENGTH  11

typedef int (*dir_iter_t)(const char* _Nonnull dirPath, const char* _Nonnull entryName);


static int  cur_year;
static int  cur_month;

static int  nlink_w;
static int  uid_w;
static int  gid_w;
static int  size_w;
static int  date_w;

static bool print_all;

static struct tm    date;

static char         buf[BUF_SIZE];
static char         path_buf[PATH_MAX];


static void file_permissions_to_text(fs_perms_t fsperms, char* _Nonnull buf)
{
    if ((fsperms & FS_PRM_R) == FS_PRM_R) {
        buf[0] = 'r';
    }
    if ((fsperms & FS_PRM_W) == FS_PRM_W) {
        buf[1] = 'w';
    }
    if ((fsperms & FS_PRM_X) == FS_PRM_X) {
        buf[2] = 'x';
    }
}

static int format_inode(const char* _Nonnull path, const char* _Nonnull entryName)
{
    fs_attr_t attr;
    
    if (fs_attr(NULL, path, &attr) != 0) {
        return -1;
    }
    
    itoa(attr.nlink, buf, 10);
    nlink_w = __max(nlink_w, strlen(buf));
    itoa(attr.uid, buf, 10);
    uid_w = __max(uid_w, strlen(buf));
    itoa(attr.gid, buf, 10);
    gid_w = __max(gid_w, strlen(buf));
    lltoa(attr.size, buf, 10);
    size_w = __max(size_w, strlen(buf));

    // Show time if the date is less than 12 months old; otherwise show date
    localtime_r(&attr.mod_time.tv_sec, &date);
    if (date.tm_year == cur_year || (date.tm_year == cur_year - 1 && date.tm_mon > cur_month)) {
        date_w = TIME_WIDTH;
    }
    else {
        date_w = DATE_WIDTH;
    }

    return 0;
}

static int print_inode(const char* _Nonnull path, const char* _Nonnull entryName)
{
    fs_attr_t attr;
    char tc;

    if (fs_attr(NULL, path, &attr) != 0) {
        return -1;
    }
    
    switch (attr.file_type) {
        case FS_FTYPE_DEV:   tc = 'h'; break;
        case FS_FTYPE_DIR:   tc = 'd'; break;
        case FS_FTYPE_PROC:  tc = 'P'; break;
        case FS_FTYPE_FIFO:   tc = 'p'; break;
        case FS_FTYPE_LNK:   tc = 'l'; break;
        default:        tc = '-'; break;
    }
    buf[0] = tc;

    for (int i = 1; i < PERMISSIONS_STRING_LENGTH; i++) {
        buf[i] = '-';
    }

    file_permissions_to_text(fs_perms_get(attr.permissions, FS_CLS_USR), &buf[1]);
    file_permissions_to_text(fs_perms_get(attr.permissions, FS_CLS_GRP), &buf[4]);
    file_permissions_to_text(fs_perms_get(attr.permissions, FS_CLS_OTH), &buf[7]);
    buf[PERMISSIONS_STRING_LENGTH - 1] = '\0';

    localtime_r(&attr.mod_time.tv_sec, &date);
        
    printf("%s %*d  %*u %*u  %*lld  ",
        buf,
        nlink_w, attr.nlink,
        uid_w, attr.uid,
        gid_w, attr.gid,
        size_w, attr.size);
    if (date_w == DATE_WIDTH) {
        printf("%s %d %d  ",
            __gc_abbrev_ymon(date.tm_mon + 1),
            date.tm_mday,
            date.tm_year + 1900);
    }
    else {
        printf("%s %d %0.2d:%0.2d  ",
            __gc_abbrev_ymon(date.tm_mon + 1),
            date.tm_mday,
            date.tm_hour,
            date.tm_min);
    }
    fputs(entryName, stdout);
    fputc('\n', stdout);

    return 0;
}


static void concat_path(char* _Nonnull path, const char* _Nonnull dir, const char* _Nonnull fileName)
{
    strcat_x(strcat_x(strcpy_x(path, dir), "/"), fileName);
}

static int format_dir_entry(const char* _Nonnull dirPath, const char* _Nonnull entryName)
{
    concat_path(path_buf, dirPath, entryName);

    return format_inode(path_buf, entryName);
}

static int print_dir_entry(const char* _Nonnull dirPath, const char* _Nonnull entryName)
{
    concat_path(path_buf, dirPath, entryName);

    return print_inode(path_buf, entryName);
}

static int iterate_dir(dir_t _Nonnull dir, const char* _Nonnull path, dir_iter_t _Nonnull cb)
{
    errno = 0;

    for (;;) {
        const dir_entry_t* dep = fs_read_directory(dir);
        
        if (dep == NULL) {
            break;
        }

        if (print_all || dep->name[0] != '.') {
            if (cb(path, dep->name) != 0) {
                break;
            }
        }
    }

    return (errno == 0) ? 0 : -1;
}

static void list_dir(const char* _Nonnull path)
{
    dir_t dir = fs_open_directory(NULL, path);

    if (dir) {
        if (iterate_dir(dir, path, format_dir_entry) == 0) {
            fs_rewind_directory(dir);
            iterate_dir(dir, path, print_dir_entry);
        }
    
        fs_close_directory(dir);
    }
}

static void list_file(const char* _Nonnull path)
{
    if (format_inode(path, path) == 0) {
        print_inode(path, path);
    }
}

static bool is_dir(const char* _Nonnull path)
{
    fs_attr_t attr;

    return (fs_attr(NULL, path, &attr) == 0 && (attr.file_type == FS_FTYPE_DIR)) ? true : false;
}


////////////////////////////////////////////////////////////////////////////////

static const char* default_path[1] = {"."};
static clap_string_array_t paths = {default_path, 1};

CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("list [-a | --all] <path>"),

    CLAP_BOOL('a', "all", &print_all, "Print entries starting with a '.'"),
    CLAP_VARARG(&paths)
);


int main(int argc, char* argv[])
{
    clap_parse(0, params, argc, argv);

    
    bool hasError = false;
    const time_t now = time(NULL);

    localtime_r(&now, &date);

    cur_year = date.tm_year;
    cur_month = date.tm_mon;

    for (size_t i = 0; i < paths.count; i++) {
        const char* path = paths.strings[i];

        if (paths.count > 1) {
            printf("%s:\n", path);
        }

        errno = 0;
        const bool isDir = is_dir(path);
        
        if (errno == 0) {
            if (isDir) {
                list_dir(path);
            }
            else {
                list_file(path);
            }
        }

        if (errno != 0) {
            clap_error(argv[0], "%s: %s", path, strerror(errno));
            hasError = true;
        }

        if (i < (paths.count - 1)) {
            putchar('\n');
        }
    }

    return (hasError) ? EXIT_FAILURE : EXIT_SUCCESS;
}
