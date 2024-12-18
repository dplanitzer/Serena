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
#include <System/Error.h>
#include <System/FilePermissions.h>
#include <System/Types.h>
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


extern void vfatal(const char* fmt, va_list ap);
extern void fatal(const char* fmt, ...);

extern errno_t cmd_create_disk(const char* _Nonnull rootPath, const char* _Nonnull dmgPath, const DiskImageFormat* _Nonnull diskImageFormat);
extern errno_t cmd_describe_disk(const char* _Nonnull dmgPath);
extern errno_t cmd_diff_disks(const char* _Nonnull dmgPath1, const char* _Nonnull dmgPath2);
extern errno_t cmd_get_disk_slice(const char* _Nonnull dmgPath, di_slice_t* _Nonnull slice, bool isHex);
extern errno_t cmd_put_disk_slice(const char* _Nonnull dmgPath, di_slice_t* _Nonnull slice);
extern errno_t cmd_list(const char* _Nonnull path, const char* _Nonnull dmgPath);

extern errno_t di_describe_diskimage(const char* _Nonnull dmgPath, DiskImage* _Nonnull pOutInfo);
extern errno_t di_lba_from_disk_addr(size_t* _Nonnull pOutLba, const DiskImage* _Nonnull info, const di_addr_t* _Nonnull addr);
extern void di_chs_from_lba(size_t* _Nonnull pOutCylinder, size_t* _Nonnull pOutHead, size_t* _Nonnull pOutSector, const DiskImage* _Nonnull info, size_t lba);


typedef struct di_direntry {
    const char*     name;
    uint64_t        fileSize;
    FilePermissions permissions;
} di_direntry;

typedef errno_t (*di_begin_directory_callback)(void* _Nullable ctx, const char* _Nonnull pPbasePath, const di_direntry* _Nonnull entry, void* pToken, void** pOutToken);
typedef errno_t (*di_end_directory_callback)(void* _Nullable ctx, void* pToken);
typedef errno_t (*di_file_callback)(void* _Nullable ctx, const char* _Nonnull pPbasePath, const di_direntry* _Nonnull entry, void* pToken);

typedef struct di_iterate_directory_callbacks {
    void * _Nullable context;
    
    di_begin_directory_callback beginDirectory;
    di_end_directory_callback   endDirectory;
    di_file_callback            file;
} di_iterate_directory_callbacks;

extern errno_t di_iterate_directory(const char* _Nonnull rootPath, const di_iterate_directory_callbacks* _Nonnull cb, void* _Nullable pInitialToken);
extern errno_t di_concat_path(const char* _Nonnull basePath, const char* _Nonnull fileName, char* _Nonnull buffer, size_t nBufferSize);

#endif /* diskimage_h */
