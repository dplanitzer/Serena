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
    kFilePermissionsScope_BitWidth = 3,

    kFilePermissionsScope_User = 2*kFilePermissionsScope_BitWidth,
    kFilePermissionsScope_Group = kFilePermissionsScope_BitWidth,
    kFilePermissionsScope_Other = 0,

    kFilePermissionsScope_Mask = 0x07,
};

// Creates a FilePermission value with permissions for a user, group and other
// permission scope.
#define FilePermissions_Make(__user, __group, __other) \
  (((__user) & kFilePermissionsScope_Mask) << kFilePermissionsScope_User) \
| (((__group) & kFilePermissionsScope_Mask) << kFilePermissionsScope_Group) \
| (((__other) & kFilePermissionsScope_Mask) << kFilePermissionsScope_Other)

// Creates a FilePermission value from a POSIX style octal number. This number
// is expected to be a 3 digit number where each digit represents one of the
// permission scopes.
#define FilePermissions_MakeFromOctal(__3_x_3_octal) \
    (__3_x_3_octal)
    
// Returns the permission bits of '__permission' that correspond to the
// permissions scope '__scope'.
#define FilePermissions_Get(__permissions, __scope) \
(((__permissions) >> (__scope)) & kFilePermissionsScope_Mask)

// Returns true if the permission '__permission' is set in the scope '__scope'
// of the permissions '__permission'.
#define FilePermissions_Has(__permissions, __scope, __permission) \
((FilePermissions_Get(__permissions, __scope) & (__permission)) == (__permission))

// Adds the permission bits '__bits' to the scope '__scope' in the file permission
// set '__permissions' 
#define FilePermissions_Add(__permissions, __scope, __bits) \
 (__permissions) |= (((__bits) & kFilePermissionsScope_Mask) << (__scope))

// Removes the permission bits '__bits' from the scope '__scope' in the file
// permission set '__permissions' 
#define FilePermissions_Remove(__permissions, __scope, __bits) \
 (__permissions) &= ~(((__bits) & kFilePermissionsScope_Mask) << (__scope))

// Replaces all permission bits in the scope '__scope' of the file permission
// set '__permissions' with the new permission bits '__bits' 
#define FilePermissions_Set(__permissions, __scope, __bits) \
 (__permissions) = ((__permissions) & ~(kFilePermissionsScope_Mask << (__scope))) | (((__bits) & kFilePermissionsScope_Mask) << (__scope))

__CPP_END

#endif /* _SYS_FILEPERMISSIONS_H */
