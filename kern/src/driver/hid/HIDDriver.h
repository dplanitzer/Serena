//
//  HIDDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef HIDDriver_h
#define HIDDriver_h

#include <driver/Driver.h>
#include <ext/try.h>
#include <kobj/Object.h>
#include <kpi/framebuffer.h>
#include <kpi/hid.h>
#include <kpi/hid_event.h>


final_class(HIDDriver, Driver);


extern errno_t HIDDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf);

// Configuring the keyboard
extern void HIDDriver_GetKeyRepeatDelays(HIDDriverRef _Nonnull self, nanotime_t* _Nullable pInitialDelay, nanotime_t* _Nullable pRepeatDelay);
extern void HIDDriver_SetKeyRepeatDelays(HIDDriverRef _Nonnull self, const nanotime_t* _Nonnull initialDelay, const nanotime_t* _Nonnull repeatDelay);


// Returns the keyboard hardware state
extern void HIDDriver_GetDeviceKeysDown(HIDDriverRef _Nonnull self, const HIDKeyCode* _Nullable pKeysToCheck, int nKeysToCheck, HIDKeyCode* _Nullable pKeysDown, int* _Nonnull nKeysDown);


// Mouse cursor state
extern errno_t HIDDriver_ObtainCursor(HIDDriverRef _Nonnull self);
extern void HIDDriver_ReleaseCursor(HIDDriverRef _Nonnull self);
extern errno_t HIDDriver_SetCursor(HIDDriverRef _Nonnull self, const void* _Nullable planes[], size_t bytesPerRow, int width, int height, pixfmt_t format, int hotSpotX, int hotSpotY);
extern void HIDDriver_ShowCursor(HIDDriverRef _Nonnull self);
extern void HIDDriver_HideCursor(HIDDriverRef _Nonnull self);
extern void HIDDriver_ObscureCursor(HIDDriverRef _Nonnull self);
extern errno_t HIDDriver_ShieldMouseCursor(HIDDriverRef _Nonnull self, int x, int y, int width, int height);


// Returns the mouse hardware state
extern void HIDDriver_GetMouseDevicePosition(HIDDriverRef _Nonnull self, int* _Nonnull pOutX, int* _Nonnull pOutY);
extern uint32_t HIDDriver_GetMouseDeviceButtonsDown(HIDDriverRef _Nonnull self);


// Event queue
extern errno_t HIDDriver_GetNextEvent(HIDDriverRef _Nonnull self, const nanotime_t* _Nonnull timeout, HIDEvent* _Nonnull evt);
extern void HIDDriver_PostEvent(HIDDriverRef _Nonnull self, HIDEventType type, did_t driverId, const HIDEventData* _Nonnull pEventData);
extern void HIDDriver_FlushEvents(HIDDriverRef _Nonnull self);

#endif /* HIDDriver_h */
