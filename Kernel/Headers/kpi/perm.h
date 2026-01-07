//
//  kpi/perm.h
//  kpi
//
//  Created by Dietmar Planitzer on 5/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_PERM_H
#define _KPI_PERM_H 1

#include <kpi/_stat.h>

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
// Note that a mode_t value type holds permission bits for all three
// permission classes.
#define S_ICWIDTH   3
#define S_ICMASK    0x07

#define S_ICUSR (2*S_ICWIDTH)
#define S_ICGRP S_ICWIDTH
#define S_ICOTH 0


// Creates a FilePermission value with permissions for a user, group and other
// permission class.
#define perm_from(__user, __group, __other) \
  (((__user) & S_ICMASK) << S_ICUSR) \
| (((__group) & S_ICMASK) << S_ICGRP) \
| (((__other) & S_ICMASK) << S_ICOTH)

// Creates a FilePermission value from a POSIX style octal number. This number
// is expected to be a 3 digit number where each digit represents one of the
// permission classes.
#define perm_from_octal(__3_x_3_octal) \
((__3_x_3_octal) & S_IFMP)
    
// Returns the permission bits of '__perms' that correspond to the
// permissions class '__class'.
#define perm_get(__perms, __class) \
(((__perms) >> (__class)) & S_ICMASK)

// Returns true if the permission '__perm' is set in the class '__class'
// of the permissions '__perm'.
#define perm_has(__perms, __class, __perm) \
((perm_get(__perms, __class) & (__perm)) == (__perm))

// Adds the permission bits '__bits' to the class '__class' in the file permission
// set '__perms' 
#define perm_add(__perms, __class, __bits) \
 (__perms) |= (((__bits) & S_ICMASK) << (__class))

// Removes the permission bits '__bits' from the class '__class' in the file
// permission set '__perms' 
#define perm_remove(__perms, __class, __bits) \
 (__perms) &= ~(((__bits) & S_ICMASK) << (__class))

// Replaces all permission bits in the class '__class' of the file permission
// set '__perms' with the new permission bits '__bits' 
#define perm_set(__perms, __class, __bits) \
 (__perms) = ((__perms) & ~(S_ICMASK << (__class))) | (((__bits) & S_ICMASK) << (__class))

#endif /* _KPI_PERM_H */
