//
//  ZorroController.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ZorroController.h"
#include "zorro_bus.h"
#include <klib/Kalloc.h>


final_class_ivars(ZorroController, Driver,
    zorro_bus_t bus;
);


errno_t ZorroController_Create(ZorroControllerRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(ZorroController, 0, pOutSelf);
}

static void ZorroController_RegisterRamExpansions(ZorroControllerRef _Nonnull self)
{
    List_ForEach(&self->bus.boards, zorro_board_t,
        if (pCurNode->type == BOARD_TYPE_RAM && pCurNode->start != NULL && pCurNode->logical_size > 0) {
            MemoryDescriptor md = {0};

            md.lower = pCurNode->start;
            md.upper = pCurNode->start + pCurNode->logical_size;
            md.type = MEM_TYPE_MEMORY;
            (void) kalloc_add_memory_region(&md);
        }
    );
}

errno_t ZorroController_onStart(ZorroControllerRef _Nonnull _Locked self)
{
    decl_try_err();

    if ((err = Driver_Publish((DriverRef)self, "zorro-bus", 0)) == EOK) {
        // Auto config the Zorro bus
        zorro_auto_config(&self->bus);


        // Find all RAM expansion boards and add them to the kalloc package
        ZorroController_RegisterRamExpansions(self);
    }

    return err;
}

class_func_defs(ZorroController, Driver,
override_func_def(onStart, ZorroController, Driver)
);
