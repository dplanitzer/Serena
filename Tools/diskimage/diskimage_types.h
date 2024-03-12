//
//  diskimage_types.h
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef diskimage_types_h
#define diskimage_types_h

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#define __posix_errors_are_defined 1
#define __time_type_is_defined 1
#define __time_spec_is_defined 1
#define __size_type_is_defined 1
#define __ABI_INTTYPES_H 1
#include <System/Error.h>
#include <System/FilePermissions.h>
#include <System/TimeInterval.h>


#ifndef _Nullable
#define _Nullable
#endif

#ifndef _Nonnull
#define _Nonnull
#endif

#ifndef _Locked
#define _Locked
#endif

#ifndef _Weak
#define _Weak
#endif


typedef uint16_t    FilePermissions;
typedef int8_t      FileType;
typedef int64_t     FileOffset;


typedef uint32_t    UserId;
typedef uint32_t    GroupId;

typedef struct _User {
    UserId  uid;
    GroupId gid;
} User;


// The Inode type.
enum {
    kFileType_RegularFile = 0,  // A regular file that stores data
    kFileType_Directory,        // A directory which stores information about child nodes
};

#endif /* diskimage_types_h */
