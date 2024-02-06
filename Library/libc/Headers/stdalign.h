//
//  stdalign.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDALIGN_H
#define _STDALIGN_H 1

#include <apollo/_cmndef.h>

__CPP_BEGIN

#ifndef __alignof_is_defined
#include <abi/_align.h>
#define __alignof_is_defined 1
#endif

__CPP_END

#endif /* _STDALIGN_H */
