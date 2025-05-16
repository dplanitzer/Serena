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
#include <stddef.h>
#include <kern/errno.h>
#include <kern/types.h>
#include <kern/stat.h>
#include <kern/uid.h>
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
    FilePermissions p;
    bool            isValid;
} di_permissions_spec_t;

typedef struct di_owner_spec {
    uid_t   uid;
    gid_t   gid;
    bool    isValid;
} di_owner_spec_t;


extern void vfatal(const char* _Nonnull fmt, va_list ap);
extern void fatal(const char* _Nonnull fmt, ...);

extern char* _Nullable create_dst_path(const char* _Nonnull srcPath, const char* _Nonnull path);

extern errno_t cmd_create(const DiskImageFormat* _Nonnull diskImageFormat, const char* _Nonnull dmgPath);
extern errno_t cmd_create_disk(const char* _Nonnull rootPath, const char* _Nonnull dmgPath, const DiskImageFormat* _Nonnull diskImageFormat);
extern errno_t cmd_describe_disk(const char* _Nonnull dmgPath);
extern errno_t cmd_diff_disks(const char* _Nonnull dmgPath1, const char* _Nonnull dmgPath2);
extern errno_t cmd_get_disk_slice(const char* _Nonnull dmgPath, di_slice_t* _Nonnull slice, bool isHex);
extern errno_t cmd_put_disk_slice(const char* _Nonnull dmgPath, di_slice_t* _Nonnull slice);

extern errno_t cmd_delete(const char* _Nonnull path, const char* _Nonnull dmgPath);
extern errno_t cmd_format(bool bQuick, FilePermissions rootDirPerms, uid_t rootDirUid, gid_t rootDirGid, const char* _Nonnull fsType, const char* _Nonnull label, const char* _Nonnull dmgPath);
extern errno_t cmd_list(const char* _Nonnull path, const char* _Nonnull dmgPath);
extern errno_t cmd_makedir(bool shouldCreateParents, FilePermissions dirPerms, uid_t uid, gid_t gid, const char* _Nonnull path, const char* _Nonnull dmgPath);
extern errno_t cmd_pull(const char* _Nonnull path, const char* _Nonnull dstPath, const char* _Nonnull dmgPath);
extern errno_t cmd_push(FilePermissions filePerms, uid_t uid, gid_t gid, const char* _Nonnull srcPath, const char* _Nonnull path, const char* _Nonnull dmgPath);

extern errno_t di_describe_diskimage(const char* _Nonnull dmgPath, DiskImage* _Nonnull pOutInfo);
extern errno_t di_lba_from_disk_addr(size_t* _Nonnull pOutLba, const DiskImage* _Nonnull info, const di_addr_t* _Nonnull addr);
extern void di_chs_from_lba(size_t* _Nonnull pOutCylinder, size_t* _Nonnull pOutHead, size_t* _Nonnull pOutSector, const DiskImage* _Nonnull info, size_t lba);

#endif /* diskimage_h */
