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
#include <System/Zorro.h>


final_class(ZorroController, Driver);

extern errno_t ZorroController_Create(ZorroControllerRef _Nullable * _Nonnull pOutSelf);


// The creation function of a Zorro board driver must conform to this prototype
// @param config board configuration. It is sufficient to just store the pointer to the configuration
// @param pOutSelf returns the driver's self
// @return success/error
typedef errno_t (*ZorroDriverCreateFunc)(const ZorroConfiguration* _Nonnull config, DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* ZorroController_h */
