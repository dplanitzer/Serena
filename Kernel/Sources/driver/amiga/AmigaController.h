//
//  AmigaController.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef AmigaController_h
#define AmigaController_h

#include <driver/PlatformController.h>


final_class(AmigaController, PlatformController);

extern errno_t AmigaController_Create(PlatformControllerRef _Nullable * _Nonnull pOutSelf);

#endif /* AmigaController_h */
