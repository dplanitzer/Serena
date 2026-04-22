//
//  proc_img.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/21/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _PROC_IMG_H_
#define _PROC_IMG_H_

#include <kpi/process.h>
#include <vm/AddressSpace.h>

#define PROC_IMG_PREFIX_SIZE    128


// Executable image builder
typedef struct proc_img {
    const char* _Nonnull                orig_path;    // original executable path
    const char* _Nullable * _Nonnull    orig_argv;    // original argv
    const char* _Nullable * _Nonnull    orig_env;     // original environment
    FileManagerRef _Nonnull             fm;

    AddressSpace                        as;
    void* _Nullable                     base;
    void* _Nullable                     entry_point;
    size_t                              arg_size;       // Size of arg_strings in terms of bytes. Includes the trailing '\0'
    char* _Nonnull                      arg_strings;    // Consecutive list of NUL-terminated process argument strings. End is marked by an empty string  
    size_t                              env_size;       // Size of env_strings in terms of bytes. Includes the trailing '\0'
    char* _Nonnull                      env_strings;    // Consecutive list of NUL-terminated process environment strings. End is marked by an empty string
    proc_ctx_t* _Nullable               ctx_base;

    char                                prefix_buf[PROC_IMG_PREFIX_SIZE];
} proc_img_t;


extern errno_t proc_img_create(FileManagerRef _Nonnull fm, const char* _Nonnull path, const char* _Nullable argv[], const char* _Nullable env[], proc_img_t* _Nullable * _Nonnull pOutImg);
extern void proc_img_destroy(proc_img_t* _Nullable pimg);

extern errno_t proc_img_load(proc_img_t* _Nonnull pimg);

#endif /* _PROC_IMG_H_ */
