//
//  kpi/directory.h
//  kpi
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_DIRECTORY_H
#define _KPI_DIRECTORY_H 1

#include <kpi/syslimits.h>
#include <kpi/types.h>

typedef struct dir_entry {
    ino_t   inid;
    char    name[NAME_MAX];
} dir_entry_t;

#endif /* _KPI_DIRECTORY_H */
