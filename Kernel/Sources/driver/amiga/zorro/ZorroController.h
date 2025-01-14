//
//  ZorroController.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef ZorroController_h
#define ZorroController_h

#include <driver/Driver.h>


final_class(ZorroController, Driver);

extern errno_t ZorroController_Create(ZorroControllerRef _Nullable * _Nonnull pOutSelf);

#endif /* ZorroController_h */
