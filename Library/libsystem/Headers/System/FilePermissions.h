//
//  FilePermissions.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_FILE_PERMISSIONS_H
#define _SYS_FILE_PERMISSIONS_H 1

#include <System/_cmndef.h>

__CPP_BEGIN

// File permissions. Every file and directory has 3 sets of permissions associated
// with it (also knows as "permission classes"):
// Owner class (of the file)
// Group class (the file is associated with)
// Other class (anyone else who isn't the owner)
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
// Note that a FilePermissions value type holds permission bits for all three
// permission classes.
enum {
    kFilePermission_Read = 0x04,
    kFilePermission_Write = 0x02,
    kFilePermission_Execute = 0x01
};

enum {
    kFilePermissionsClass_BitWidth = 3,

    kFilePermissionsClass_User = 2*kFilePermissionsClass_BitWidth,
    kFilePermissionsClass_Group = kFilePermissionsClass_BitWidth,
    kFilePermissionsClass_Other = 0,

    kFilePermissionsClass_Mask = 0x07,
};

// Creates a FilePermission value with permissions for a user, group and other
// permission class.
#define FilePermissions_Make(__user, __group, __other) \
  (((__user) & kFilePermissionsClass_Mask) << kFilePermissionsClass_User) \
| (((__group) & kFilePermissionsClass_Mask) << kFilePermissionsClass_Group) \
| (((__other) & kFilePermissionsClass_Mask) << kFilePermissionsClass_Other)

// Creates a FilePermission value from a POSIX style octal number. This number
// is expected to be a 3 digit number where each digit represents one of the
// permission classes.
#define FilePermissions_MakeFromOctal(__3_x_3_octal) \
    (__3_x_3_octal)
    
// Returns the permission bits of '__permission' that correspond to the
// permissions class '__class'.
#define FilePermissions_Get(__permissions, __class) \
(((__permissions) >> (__class)) & kFilePermissionsClass_Mask)

// Returns true if the permission '__permission' is set in the class '__class'
// of the permissions '__permission'.
#define FilePermissions_Has(__permissions, __class, __permission) \
((FilePermissions_Get(__permissions, __class) & (__permission)) == (__permission))

// Adds the permission bits '__bits' to the class '__class' in the file permission
// set '__permissions' 
#define FilePermissions_Add(__permissions, __class, __bits) \
 (__permissions) |= (((__bits) & kFilePermissionsClass_Mask) << (__class))

// Removes the permission bits '__bits' from the class '__class' in the file
// permission set '__permissions' 
#define FilePermissions_Remove(__permissions, __class, __bits) \
 (__permissions) &= ~(((__bits) & kFilePermissionsClass_Mask) << (__class))

// Replaces all permission bits in the class '__class' of the file permission
// set '__permissions' with the new permission bits '__bits' 
#define FilePermissions_Set(__permissions, __class, __bits) \
 (__permissions) = ((__permissions) & ~(kFilePermissionsClass_Mask << (__class))) | (((__bits) & kFilePermissionsClass_Mask) << (__class))

__CPP_END

#endif /* _SYS_FILE_PERMISSIONS_H */
