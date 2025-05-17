//
//  kpi/dirent.h
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_DIRENT_H
#define _KPI_DIRENT_H 1

#include <kpi/syslimits.h>
#include <kpi/types.h>

typedef struct dirent {
    ino_t   inid;
    char    name[__PATH_COMPONENT_MAX];
} dirent_t;

#endif /* _KPI_DIRENT_H */
