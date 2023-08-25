//
//  stdalign.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDALIGN_H
#define _STDALIGN_H 1

#ifndef __alignof_is_defined
#define _Alignof(type) __alignof(type)
#define alignof(type) __alignof(type)
#define __alignof_is_defined 1
#endif

#endif /* _STDALIGN_H */
