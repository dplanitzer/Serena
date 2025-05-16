//
//  sys/vm.h
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_VM_H
#define _SYS_VM_H 1

#include <kern/_cmndef.h>
#include <stddef.h>

__CPP_BEGIN

extern int vm_alloc(size_t nbytes, void* _Nullable * _Nonnull ptr);

__CPP_END

#endif /* _SYS_VM_H */
