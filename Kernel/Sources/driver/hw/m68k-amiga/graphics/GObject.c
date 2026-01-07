//
//  GObject.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/8/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "GObject.h"
#include "ColorTable.h"
#include "Surface.h"
#include <kern/assert.h>


void GObject_DelRef(void* _Nullable self0)
{
    if (self0) {
        GObject* self = self0;

        self->refCount--;
        if (self->refCount == 0) {
            assert(self->chain.next == NULL && self->chain.prev == NULL);
        
            switch (self->type) {
                case kGObject_ColorTable:
                    ColorTable_Destroy(self0);
                    break;

                case kGObject_Surface:
                    Surface_Destroy(self0);
                    break;

                default:
                    abort();
            }
        }
    }
}
