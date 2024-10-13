//
//  ZorroController.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ZorroController.h"
#include "zorro_bus.h"
#include <driver/DriverCatalog.h>
#include <klib/Kalloc.h>


final_class_ivars(ZorroController, Driver,
    ExpansionBus    zorroBus;
);


errno_t ZorroController_Create(ZorroControllerRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(ZorroController, kDriverModel_Sync, (DriverRef*)pOutSelf);
}

errno_t ZorroController_start(ZorroControllerRef _Nonnull self)
{
    // Auto config the Zorro bus
    zorro_auto_config(&self->zorroBus);


    // Find all RAM expansion boards and add them to the kalloc package
    for (int i = 0; i < self->zorroBus.board_count; i++) {
        const ExpansionBoard* board = &self->zorroBus.board[i];
       
        if (board->type == EXPANSION_TYPE_RAM && board->start != NULL && board->logical_size > 0) {
            MemoryDescriptor md = {0};

            md.lower = board->start;
            md.upper = board->start + board->logical_size;
            md.type = MEM_TYPE_MEMORY;
            (void) kalloc_add_memory_region(&md);
        }
    }

    return EOK;
}

class_func_defs(ZorroController, Driver,
override_func_def(start, ZorroController, Driver)
);
