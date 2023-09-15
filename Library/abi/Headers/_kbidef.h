//
//  _kbidef.h
//  Apollo
//
//  Created by Dietmar Planitzer on 9/14/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __KBIDEF_H
#define __KBIDEF_H 1

#include <_nulldef.h>
#include <_dmdef.h>

// The process arguments descriptor is stored in the process address space and
// it contains a pointer to the base of the command line arguments and environment
// variables tables. These tables store pointers to nul-terminated strings and
// the last entry in the table contains a NULL.
// This data structure is set up by the kernel when it processes an exec() or
// spawn() request. Once set up the kernel neither reads nor writes to this area.
struct __process_arguments_t {
    __size_t                    version;    // sizeof(__process_arguments_t)
    __size_t                    reserved;
    __size_t                    arguments_size; // Size of the area that holds all of __process_arguments_t + argv + envp
    __size_t                    argc;           // Number of command line arguments passed to the process. Argv[0] holds the path to the process through which it was started
    char* _Nullable * _Nonnull  argv;           // Pointer to the base of the command line arguments table. Last entry is NULL
    char* _Nullable * _Nonnull  envp;           // Pointer to the base of the environment table. Last entry holds NULL.
    void* _Nonnull              image_base;     // Pointer to the base of the executable header
};

#endif /* __KBIDEF_H */
