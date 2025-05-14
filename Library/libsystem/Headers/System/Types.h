//
//  Types.h
//  libsystem
//
//  Created by Dietmar Planitzer on 10/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_TYPES2_H
#define _SYS_TYPES2_H 1

#include <System/abi/_dmdef.h>
#include <System/abi/_bool.h>
#include <System/abi/_inttypes.h>
#include <System/_null.h>
#include <System/_errno.h>
#include <System/_cmndef.h>
#ifndef __SYSTEM_SHIM__
#include <sys/types.h>
#else
#include <kern/types.h>
#endif

__CPP_BEGIN


typedef uint16_t    FilePermissions;
typedef int8_t      FileType;


#ifndef OFF_MIN
#define OFF_MIN  __OFF_MIN
#endif
#ifndef OFF_MAX
#define OFF_MAX  __OFF_MAX
#endif
#ifndef OFF_WIDTH
#define OFF_WIDTH __OFF_WIDTH
#endif


#ifndef SSIZE_MIN
#define SSIZE_MIN  __SSIZE_MIN
#endif
#ifndef SSIZE_MAX
#define SSIZE_MAX  __SSIZE_MAX
#endif
#ifndef SSIZE_WIDTH
#define SSIZE_WIDTH __SSIZE_WIDTH
#endif


#ifndef SIZE_MAX
#define SIZE_MAX __SIZE_MAX
#endif
#ifndef SIZE_WIDTH
#define SIZE_WIDTH __SIZE_WIDTH
#endif

__CPP_END

#endif /* _SYS_TYPES2_H */
