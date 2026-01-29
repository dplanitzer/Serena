//
//  kpi/dirent.h
//  kpi
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_DIRENT_H
#define _KPI_DIRENT_H 1

#include <kpi/syslimits.h>
#include <kpi/types.h>

struct dirent {
    ino_t   inid;
    char    name[NAME_MAX];
};

#endif /* _KPI_DIRENT_H */
