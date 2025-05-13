//
//  types.h
//  libc
//
//  Created by Dietmar Planitzer on 5/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H 1

#include <System/abi/_dmdef.h>
#include <System/abi/_size.h>
#include <System/_cmndef.h>
#include <System/_null.h>
#include <System/_offsetof.h>

__CPP_BEGIN

// Represents a logical block address and count
typedef size_t  blkno_t;
typedef blkno_t blkcnt_t;

__CPP_END

#endif /* _SYS_TYPES_H */
