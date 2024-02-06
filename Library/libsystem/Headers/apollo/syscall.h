//
//  syscall.h
//  libsystem
//
//  Created by Dietmar Planitzer on 9/2/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H 1

#include <apollo/_cmndef.h>
#include <abi/_syscall.h>

__CPP_BEGIN

extern int _syscall(int scno, ...);

__CPP_END

#endif /* _SYS_SYSCALL_H */
