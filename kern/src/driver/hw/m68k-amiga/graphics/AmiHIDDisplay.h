//
//  AmiHIDDisplay.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef AmiHIDDisplay_h
#define AmiHIDDisplay_h

#include <driver/hid/IOHIDDisplay.h>


open_class(AmiHIDDisplay, IOHIDDisplay,
    int     cursorBufferId;
    int16_t cursorWidth;
    int16_t cursorHeight;
);
open_class_funcs(AmiHIDDisplay, IOHIDDisplay,
);

extern errno_t AmiHIDDisplay_Create(AmiHIDDisplayRef _Nullable * _Nonnull pOutSelf);

#endif /* AmiHIDDisplay_h */
