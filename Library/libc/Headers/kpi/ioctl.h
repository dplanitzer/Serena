//
//  kpi/ioctl.h
//  libc
//
//  Created by Dietmar Planitzer on 4/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_IOCTL_H
#define _KPI_IOCTL_H 1

#define IOResourceCommand(__cmd) (__cmd)


// Driver subclasses define their respective commands based on this number.
#define kDriverCommand_SubclassBase  IOResourceCommand(256)

#endif /* _KPI_IOCTL_H */
