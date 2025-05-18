//
//  kpi/_stat.h
//  libc
//
//  Created by Dietmar Planitzer on 5/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_PRIV_STAT_H
#define _KPI_PRIV_STAT_H 1

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

#define S_IRWX  07
#define S_IR    04
#define S_IW    02
#define S_IX    01

// Inode type bits in st_mode
#define S_IFMT      0xff000000
#define __S_IFST    24

// Permission bits in st_mode
#define S_IFMP      (~S_IFMT)

// Make a st_mode from a file type and file permissions
#define __S_MKMODE(__ftype, __fperm) ((__ftype) | ((__fperm) & S_IFMP))

#endif /* _KPI_PRIV_STAT_H */
