//
//  HIDChannel.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef HIDChannel_h
#define HIDChannel_h

#include <driver/DriverChannel.h>
#include <sys/hidevent.h>
#include <System/TimeInterval.h>


open_class(HIDChannel, DriverChannel,
);
open_class_funcs(HIDChannel, DriverChannel,
);


extern errno_t HIDChannel_Create(DriverRef _Nonnull pDriver, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutSelf);

extern errno_t HIDChannel_GetNextEvent(IOChannelRef _Nonnull self, TimeInterval timeout, HIDEvent* _Nonnull evt);

#endif /* HIDChannel_h */
