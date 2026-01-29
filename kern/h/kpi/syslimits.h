//
//  kpi/syslimits.h
//  kpi
//
//  Created by Dietmar Planitzer on 9/15/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_SYSLIMITS_H
#define _KPI_SYSLIMITS_H 1

// Maximum size of a single command line argument or environment entry
#define __ARG_STRLEN_MAX 16384

// Maximum size of the command line arguments and environment that can be passed to a process
#define __ARG_MAX 262144

// Max length of a path including the terminating NUL character
#define PATH_MAX 1024

// Max length of a path component (file or directory name) without the terminating NUL character
#define NAME_MAX 255

// Max number of times an inode can be linked into a file hierarchy
#define NLINK_MAX ((int)0x7fffffff)

#endif /* _KPI_SYSLIMITS_H */
