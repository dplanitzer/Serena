//
//  kern/limits.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_LIMITS_H
#define _KERN_LIMITS_H 1

#include <limits.h>

#ifndef OFF_MIN
#define OFF_MIN  __OFF_MIN
#endif
#ifndef OFF_MAX
#define OFF_MAX  __OFF_MAX
#endif
#ifndef OFF_WIDTH
#define OFF_WIDTH __OFF_WIDTH
#endif


#ifndef SSIZE_MIN
#define SSIZE_MIN  __SSIZE_MIN
#endif
#ifndef SSIZE_MAX
#define SSIZE_MAX  __SSIZE_MAX
#endif
#ifndef SSIZE_WIDTH
#define SSIZE_WIDTH __SSIZE_WIDTH
#endif


#ifndef SIZE_MAX
#define SIZE_MAX __SIZE_MAX
#endif
#ifndef SIZE_WIDTH
#define SIZE_WIDTH __SIZE_WIDTH
#endif

#endif /* _KERN_LIMITS_H */
