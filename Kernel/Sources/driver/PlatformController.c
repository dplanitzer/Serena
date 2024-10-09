//
//  PlatformController.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "PlatformController.h"
#include <klib/Log.h>


// Should be invoked by the platform specific subclass to inform the kernel that
// the console is available now. Initializes the kernel logging services and 
// prints the boot banner.
void PlatformController_NoteConsoleAvailable(PlatformControllerRef _Nonnull self)
{
    // Initialize the kernel print services
    print_init();


    // Boot message
    print("\033[36mSerena OS v0.2.0-alpha\033[0m\nCopyright 2023, Dietmar Planitzer.\n\n");


    // Debug printing
    //PrintClasses();
}


class_func_defs(PlatformController, Driver,
);
