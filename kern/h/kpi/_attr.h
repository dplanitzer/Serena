//
//  kpi/_attr.h
//  kpi
//
//  Created by Dietmar Planitzer on 5/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef __KPI_ATTR_H
#define __KPI_ATTR_H 1

#define S_IRWXU 0700
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXG 070
#define S_IRGRP 040
#define S_IWGRP 020
#define S_IXGRP 010
#define S_IRWXO 07
#define S_IROTH 04
#define S_IWOTH 02
#define S_IXOTH 01

#define S_IRWX      07
#define S_IREAD     04
#define S_IWRITE    02
#define S_IEXEC     01


// Inode type bits in mode
#define S_IFMT      0xff000000

// Permission bits in mode
#define S_IFMP      (~S_IFMT)

#endif /* __KPI_ATTR_H */
