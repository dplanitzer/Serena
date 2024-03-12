//
//  FilePermissions.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_FILEPERMISSIONS_H
#define _SYS_FILEPERMISSIONS_H 1

#include <System/_cmndef.h>

__CPP_BEGIN

// File permissions. Every file and directory has 3 sets of permissions associated
// with it (also knows as "permission scopes"):
// Owner (of the file)
// Group (the file is associated with)
// Anyone else
//
// The meaning of the permission bits for files are:
// R    Allow reading of the file contents
// W    Allow writing/updating the file contents
// X    Allow executing the file. The file must contain data in the executable format
//
// The meaning of the permission bits for directories are:
// R    Allow reading the directory listing
// W    Allow adding/removing directory entries
// X    Allow searching the directory listing
//
// Note that a FilePermission value holds permission bits for all three permission
// scopes.
enum {
    kFilePermission_Read = 0x04,
    kFilePermission_Write = 0x02,
    kFilePermission_Execute = 0x01
};

enum {
    kFilePermissionScope_BitWidth = 3,

    kFilePermissionScope_User = 2*kFilePermissionScope_BitWidth,
    kFilePermissionScope_Group = kFilePermissionScope_BitWidth,
    kFilePermissionScope_Other = 0,

    kFilePermissionScope_Mask = 0x07,
};

// Creates a FilePermission value with permissions for a user, group and other
// permission scope.
#define FilePermissions_Make(__user, __group, __other) \
  (((__user) & kFilePermissionScope_Mask) << kFilePermissionScope_User) \
| (((__group) & kFilePermissionScope_Mask) << kFilePermissionScope_Group) \
| (((__other) & kFilePermissionScope_Mask) << kFilePermissionScope_Other)

// Creates a FilePermission value from a POSIX style octal number. This number
// is expected to be a 3 digit number where each digit represents one of the
// permission scopes.
#define FilePermissions_MakeFromOctal(__3_x_3_octal) \
    (__3_x_3_octal)
    
// Returns the permission bits of '__permission' that correspond to the
// permissions scope '__scope'.
#define FilePermissions_Get(__permissions, __scope) \
(((__permissions) >> (__scope)) & kFilePermissionScope_Mask)

// Replaces the permission bits of the scope '__scope' in '__permission' with the
// permission bits '__bits'. 
#define FilePermissions_Set(__permissions, __scope, __bits) \
((__permissions) & ~(kFilePermissionsScope_Mask << (__scope))) | (((__bits) & kFilePermissionsScope_Mask) << (__scope))

__CPP_END

#endif /* _SYS_FILEPERMISSIONS_H */
