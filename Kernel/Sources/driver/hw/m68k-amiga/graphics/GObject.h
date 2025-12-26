//
//  GObject.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/1/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef GObject_h
#define GObject_h

#include <ext/queue.h>
#include <kern/types.h>

enum {
    kGObject_Surface = 1,
    kGObject_ColorTable,
};

typedef struct GObject {
    ListNode    chain;
    int         id;
    int32_t     refCount;
    int16_t     type;
} GObject;


#define GObject_AddRef(__self) \
(((GObject*)(__self))->refCount++)

extern void GObject_DelRef(void* _Nullable self);


#define GObject_GetId(__self) \
(((GObject*)(__self))->id)

#define GObject_GetType(__self) \
(((GObject*)(__self))->type)


#define GObject_GetChainPtr(__self) \
&(((GObject*)(__self))->chain)

#endif /* GObject_h */
