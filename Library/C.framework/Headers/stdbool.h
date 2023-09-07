//
//  stdbool.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDBOOL_H
#define _STDBOOL_H 1

#include <_cmndef.h>
#include <_booldef.h>

__CPP_BEGIN

#ifndef __bool_true_false_are_defined

#define bool __bool
#define true __true
#define false __false
#define __bool_true_false_are_defined 1
#endif

__CPP_END

#endif /* _STDBOOL_H */
