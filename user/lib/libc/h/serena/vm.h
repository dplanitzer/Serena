//
//  serena/vm.h
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SERENA_VM_H
#define _SERENA_VM_H 1

#include <_cmndef.h>
#include <stddef.h>

__CPP_BEGIN

extern int vm_allocate(size_t nbytes, void* _Nullable * _Nonnull ptr);

__CPP_END

#endif /* _SERENA_VM_H */
