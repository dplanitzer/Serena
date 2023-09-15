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
struct __process_arguments_t {
    char* _Nullable * _Nonnull  argv;
    __size_t                    argc;
    
    char* _Nullable * _Nonnull  envp;
    __size_t                    envc;

    // Pointer to the base of the executable header
    void* _Nonnull              image_base;
};

#endif /* __KBIDEF_H */
