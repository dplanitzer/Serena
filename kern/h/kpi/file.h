//
//  kpi/file.h
//  kpi
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_FILE_H
#define _KPI_FILE_H 1

// Tells fs_settimes() to set the file timestamp to the current time. Assign to
// the tv_nsec field.
#define FS_TIME_NOW   -1

// Tells fs_settimes() to set the file timestamp to leave the file timestamp
// unchanged. Assign to the tv_nsec field.
#define FS_TIME_OMIT  -2


// Order of the fs_settimes() timestamps
#define FS_TIMFLD_ACC   0   /* Access time */
#define FS_TIMFLD_MOD   1   /* Modification time */


// __fs_unlink() system call mode
#define __FS_ULNK_ANY       0
#define __FS_ULNK_FIL_ONLY  1
#define __FS_ULNK_DIR_ONLY  2

#endif /* _KPI_FILE_H */
