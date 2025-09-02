//
//  GObject.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/1/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef GObject_h
#define GObject_h

#include <kern/types.h>
#include <klib/List.h>

enum {
    kGObject_Surface = 1,
    kGObject_ColorTable,
};

typedef struct GObject {
    ListNode    chain;
    int         id;
    int16_t     type;
    int16_t     useCount;
} GObject;


#define GObject_AddUse(__self) \
(((GObject*)(__self))->useCount++)

#define GObject_DelUse(__self) \
(--((GObject*)(__self))->useCount == 0)

#define GObject_InUse(__self) \
(((GObject*)(__self))->useCount > 0)


#define GObject_GetId(__self) \
(((GObject*)(__self))->id)

#define GObject_GetType(__self) \
(((GObject*)(__self))->type)


#define GObject_GetChainPtr(__self) \
&(((GObject*)(__self))->chain)

#endif /* GObject_h */
