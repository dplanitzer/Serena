//
//  diskimage.h
//  diskimage
//
//  Created by Dietmar Planitzer on 3/14/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef diskimage_h
#define diskimage_h

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <ext/try.h>
#include <kpi/file.h>
#include <kpi/types.h>
#include <kpi/uid.h>
#ifdef _WIN32
#include <Windows.h>
#endif
#include "DiskImageFormat.h"


typedef enum di_slice_type {
    di_slice_type_empty,
    di_slice_type_sector,
    di_slice_type_track
} di_slice_type;

typedef enum di_addr_type {
    di_addr_type_lba,
    di_addr_type_chs
} di_addr_type;

typedef struct di_chs {
    size_t  cylinder;
    size_t  head;
    size_t  sector;
} di_chs_t;

typedef struct di_addr {
    di_addr_type    type;
    union {
        size_t      lba;
        di_chs_t    chs;
    }               u;
} di_addr_t;

typedef struct di_slice {
    di_slice_type   type;
    di_addr_t       start;
} di_slice_t;

typedef struct di_permissions_spec {
    fs_perms_t  p;
    bool        isValid;
} di_permissions_spec_t;

typedef struct di_owner_spec {
    uid_t   uid;
    gid_t   gid;
    bool    isValid;
} di_owner_spec_t;


typedef enum {
    HOST_PATH = 0,
    DMG_PATH,
    POSIX_PATH,
    WIN32_PATH
} di_path_type;


#ifdef _WIN32
#define HOST_PATH_MAX   MAX_PATH
#else
#define HOST_PATH_MAX   PATH_MAX
#endif
#define DMG_PATH_MAX    1024        // kpi/syslimits.h
#define LPC_SIZE        FILENAME_MAX

#define COPY_BUF_SIZE   4096


extern char g_copy_buf[COPY_BUF_SIZE];
extern char g_host_path[HOST_PATH_MAX];
extern char g_dmg_path[DMG_PATH_MAX];
extern char g_last_path_comp[LPC_SIZE];



extern errno_t set_host_path(const char* _Nonnull path);
extern errno_t set_dmg_path(const char* _Nonnull path);

extern bool is_host_directory(const char* _Nonnull path);

extern ssize_t path_append(di_path_type path_type, char* _Nullable buf, size_t buf_size, const char* _Nonnull component);
extern ssize_t last_path_component(di_path_type path_type, char* _Nullable buf, size_t buf_size, const char* _Nonnull path);

extern errno_t cmd_create(const DiskImageFormat* _Nonnull diskImageFormat, const char* _Nonnull dmgPath);
extern errno_t cmd_create_disk(const char* _Nonnull rootPath, const char* _Nonnull dmgPath, const DiskImageFormat* _Nonnull diskImageFormat);
extern errno_t cmd_describe_disk(const char* _Nonnull dmgPath);
extern errno_t cmd_diff_disks(const char* _Nonnull dmgPath1, const char* _Nonnull dmgPath2);
extern errno_t cmd_get_disk_slice(const char* _Nonnull dmgPath, di_slice_t* _Nonnull slice, bool isHex);
extern errno_t cmd_put_disk_slice(const char* _Nonnull dmgPath, di_slice_t* _Nonnull slice);

extern errno_t cmd_delete(const char* _Nonnull path, const char* _Nonnull dmgPath);
extern errno_t cmd_format(bool bQuick, fs_perms_t rootDirPerms, uid_t rootDirUid, gid_t rootDirGid, const char* _Nonnull fsType, const char* _Nonnull label, const char* _Nonnull dmgPath);
extern errno_t cmd_list(const char* _Nonnull path, const char* _Nonnull dmgPath);
extern errno_t cmd_makedir(bool shouldCreateParents, fs_perms_t dirPerms, uid_t uid, gid_t gid, const char* _Nonnull path, const char* _Nonnull dmgPath);
extern errno_t cmd_pull(const char* _Nonnull path, const char* _Nonnull dstPath, const char* _Nonnull dmgPath);
extern errno_t cmd_push(fs_perms_t fsperms, uid_t uid, gid_t gid, const char* _Nonnull srcPath, const char* _Nonnull path, const char* _Nonnull dmgPath);

extern errno_t di_describe_diskimage(const char* _Nonnull dmgPath, DiskImage* _Nonnull pOutInfo);
extern errno_t di_lba_from_disk_addr(size_t* _Nonnull pOutLba, const DiskImage* _Nonnull info, const di_addr_t* _Nonnull addr);
extern void di_chs_from_lba(size_t* _Nonnull pOutCylinder, size_t* _Nonnull pOutHead, size_t* _Nonnull pOutSector, const DiskImage* _Nonnull info, size_t lba);

#if defined(__APPLE__)
extern char * _Nullable strdup(const char * _Nonnull src);
#endif

#endif /* diskimage_h */
