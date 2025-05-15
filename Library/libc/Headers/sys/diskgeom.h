//
//  sys/diskgeom.h
//  libc
//
//  Created by Dietmar Planitzer on 10/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_DISKGEOM_H
#define _SYS_DISKGEOM_H 1

#include <stddef.h>

// Disk geometry information
typedef struct diskgeom {
    size_t  headsPerCylinder;
    size_t  sectorsPerTrack;
    size_t  cylindersPerDisk;
    size_t  sectorSize;
} diskgeom_t;

#endif /* _SYS_DISKGEOM_H */
