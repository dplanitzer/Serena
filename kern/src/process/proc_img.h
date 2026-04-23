//
//  proc_img.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/21/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _PROC_IMG_H_
#define _PROC_IMG_H_

#include <kpi/attr.h>
#include <kpi/process.h>
#include <vm/AddressSpace.h>

#define PROC_IMG_PREFIX_SIZE    127


// Executable image builder
typedef struct proc_img {
    const char* _Nonnull                orig_path;    // original executable path
    const char* _Nullable * _Nonnull    orig_argv;    // original argv (points initially to argv_buf[2])
    const char* _Nullable * _Nonnull    orig_env;     // original environment

    FileManagerRef _Nonnull             fm;
    IOChannelRef _Nonnull               file;
    fs_attr_t                           file_attr;
    size_t                              prefix_length;  // true length of the file prefix (number of bytes actually read in)

    AddressSpace                        as;
    void* _Nullable                     base;
    void* _Nullable                     entry_point;
    size_t                              arg_size;       // Size of arg_strings in terms of bytes. Includes the trailing '\0'
    char* _Nonnull                      arg_strings;    // Consecutive list of NUL-terminated process argument strings. End is marked by an empty string  
    size_t                              env_size;       // Size of env_strings in terms of bytes. Includes the trailing '\0'
    char* _Nonnull                      env_strings;    // Consecutive list of NUL-terminated process environment strings. End is marked by an empty string
    proc_ctx_t* _Nullable               ctx_base;

    int                                 nesting_level;  // How often we've done a ldr_script indirection (currently limited to 1)
    
    char                                prefix_buf[PROC_IMG_PREFIX_SIZE + 1];   // proc_img_load() reads PROC_IMG_PREFIX_SIZE bytes and provides space for an extra byte that can be set to '\0' when needed
    const char* _Nullable               argv_buf[2];    // buffer big enough to hold a copy of the original argv + 2 extra slots for arg0 override
} proc_img_t;


extern errno_t proc_img_create(FileManagerRef _Nonnull fm, const char* _Nonnull path, const char* _Nullable argv[], const char* _Nullable env[], proc_img_t* _Nullable * _Nonnull pOutImg);
extern void proc_img_destroy(proc_img_t* _Nullable pimg);

// Replaces the current argv[0] entry with the given interpreter path, optional
// interpreter argument and the original executable file path.
// NOTE: the passed in string points have to point to either the prefix buffer
// contents or the original path that was passed to proc_img_create(). 
extern errno_t proc_img_replace_arg0(proc_img_t* _Nonnull pimg, const char* _Nonnull interp, const char* _Nullable interp_arg, const char* _Nonnull script_path);

// Opens the executable file at 'path' and makes it the current file.
extern errno_t proc_img_open_file(proc_img_t* _Nonnull pimg, const char* _Nonnull path);

// Loads an executable from the given executable file.
extern errno_t proc_img_load(proc_img_t* _Nonnull pimg);

#endif /* _PROC_IMG_H_ */
