//
//  ext/errno.h
//  diskimage
//
//  Created by Dietmar Planitzer on 1/6/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _DI_ERRNO_H
#define _DI_ERRNO_H 1

#include <errno.h>

#define EOK 0

#ifndef EACCESS
#define EACCESS EACCES
#endif
#ifndef ENOTIOCTLCMD
#define ENOTIOCTLCMD EINVAL
#endif

#endif /* _DI_ERRNO_H */
