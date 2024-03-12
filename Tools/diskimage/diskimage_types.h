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
#include <windows.h>
#define __posix_errors_are_defined 1
#define __time_type_is_defined 1
#define __time_spec_is_defined 1
#define __size_type_is_defined 1
#define __ABI_INTTYPES_H 1
#include <System/abi/_ssize.h>
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

//typedef struct Filesystem {
//    int dummy;
//} Filesystem;
//typedef Filesystem* FilesystemRef;
typedef struct FilesystemMethodTable {
    int dummy;
} FilesystemMethodTable;

typedef void* FilesystemRef;


typedef struct _Class {
    size_t  instanceSize;
} Class;
typedef struct _Class* ClassRef;


// Defines an opaque class. An opaque class supports limited subclassing only.
// Overriding methods is supported but adding ivars is not. This macro should
// be placed in the publicly accessible header file of the class.
#define OPAQUE_CLASS(__name, __superName) \
extern Class k##__name##Class; \
struct _##__name; \
typedef struct _##__name* __name##Ref

// Defines the ivars of an opaque class. This macro should be placed either in
// the class implementation file or a private class header file.
#define CLASS_IVARS(__name, __super, __ivar_decls) \
typedef struct _##__name { __ivar_decls } __name

#define CLASS_METHODS(__name, __super, ...) \
Class k##__name##Class = {sizeof(__name)}

#define METHOD_IMPL(__name, __className)
#define OVERRIDE_METHOD_IMPL(__name, __className, __superClassName)

#endif /* diskimage_types_h */
