//
//  kpi/fs_perms.h
//  kpi
//
//  Created by Dietmar Planitzer on 5/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_FS_PERMS_H
#define _KPI_FS_PERMS_H 1

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
// Note that a fs_perms_t value type holds permission bits for all three
// permission classes.
#define FS_USR_RWX  0700
#define FS_USR_R    0400
#define FS_USR_W    0200
#define FS_USR_X    0100
#define FS_GRP_RWX  070
#define FS_GRP_R    040
#define FS_GRP_W    020
#define FS_GRP_X    010
#define FS_OTH_RWX  07
#define FS_OTH_R    04
#define FS_OTH_W    02
#define FS_OTH_X    01

#define FS_PRM_RWX  07
#define FS_PRM_R    04
#define FS_PRM_W    02
#define FS_PRM_X    01


#define FS_CLS_WIDTH  3
#define FS_CLS_MASK   0x07

#define FS_CLS_USR    (2*FS_CLS_WIDTH)
#define FS_CLS_GRP    FS_CLS_WIDTH
#define FS_CLS_OTH    0


// Creates a FilePermission value with permissions for a user, group and other
// permission class.
#define fs_perms_from(__user, __group, __other) \
  (((__user) & FS_CLS_MASK) << FS_CLS_USR) \
| (((__group) & FS_CLS_MASK) << FS_CLS_GRP) \
| (((__other) & FS_CLS_MASK) << FS_CLS_OTH)

// Creates a FilePermission value from a POSIX style octal number. This number
// is expected to be a 3 digit number where each digit represents one of the
// permission classes.
#define fs_perms_from_octal(__3_x_3_octal) \
((__3_x_3_octal) & 0777)
    
// Returns the permission bits of '__perms' that correspond to the
// permissions class '__class'.
#define fs_perms_get(__perms, __class) \
(((__perms) >> (__class)) & FS_CLS_MASK)

// Returns true if the permission '__perm' is set in the class '__class'
// of the permissions '__perm'.
#define fs_perms_has(__perms, __class, __perm) \
((fs_perms_get(__perms, __class) & (__perm)) == (__perm))

// Adds the permission bits '__bits' to the class '__class' in the file permission
// set '__perms' 
#define fs_perms_add(__perms, __class, __bits) \
 (__perms) |= (((__bits) & FS_CLS_MASK) << (__class))

// Removes the permission bits '__bits' from the class '__class' in the file
// permission set '__perms' 
#define fs_perms_remove(__perms, __class, __bits) \
 (__perms) &= ~(((__bits) & FS_CLS_MASK) << (__class))

// Replaces all permission bits in the class '__class' of the file permission
// set '__perms' with the new permission bits '__bits' 
#define fs_perms_set(__perms, __class, __bits) \
 (__perms) = ((__perms) & ~(FS_CLS_MASK << (__class))) | (((__bits) & FS_CLS_MASK) << (__class))

#endif /* _KPI_FS_PERMS_H */
