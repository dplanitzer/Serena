//
//  HIDManager.h
//  kernel
//
//  Created by Dietmar Planitzer on 1/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef HIDManager_h
#define HIDManager_h

#include <kobj/Object.h>
#include <kern/errno.h>
#include <kpi/fb.h>
#include <kpi/hid.h>
#include <kpi/hidevent.h>


extern HIDManagerRef _Nonnull gHIDManager;

extern errno_t HIDManager_Create(HIDManagerRef _Nullable * _Nonnull pOutSelf);
extern errno_t HIDManager_Start(HIDManagerRef _Nonnull self);


// Configuring the keyboard
extern void HIDManager_GetKeyRepeatDelays(HIDManagerRef _Nonnull self, struct timespec* _Nullable pInitialDelay, struct timespec* _Nullable pRepeatDelay);
extern void HIDManager_SetKeyRepeatDelays(HIDManagerRef _Nonnull self, const struct timespec* _Nonnull initialDelay, const struct timespec* _Nonnull repeatDelay);


// Returns the keyboard hardware state
extern void HIDManager_GetDeviceKeysDown(HIDManagerRef _Nonnull self, const HIDKeyCode* _Nullable pKeysToCheck, int nKeysToCheck, HIDKeyCode* _Nullable pKeysDown, int* _Nonnull nKeysDown);


// Mouse cursor state
extern errno_t HIDManager_ObtainCursor(HIDManagerRef _Nonnull self);
extern void HIDManager_ReleaseCursor(HIDManagerRef _Nonnull self);
extern errno_t HIDManager_SetCursor(HIDManagerRef _Nonnull self, const void* _Nullable planes[], size_t bytesPerRow, int width, int height, PixelFormat format, int hotSpotX, int hotSpotY);
extern void HIDManager_ShowCursor(HIDManagerRef _Nonnull self);
extern void HIDManager_HideCursor(HIDManagerRef _Nonnull self);
extern void HIDManager_ObscureCursor(HIDManagerRef _Nonnull self);
extern errno_t HIDManager_ShieldMouseCursor(HIDManagerRef _Nonnull self, int x, int y, int width, int height);


// Returns the mouse hardware state
extern void HIDManager_GetMouseDevicePosition(HIDManagerRef _Nonnull self, int* _Nonnull pOutX, int* _Nonnull pOutY);
extern uint32_t HIDManager_GetMouseDeviceButtonsDown(HIDManagerRef _Nonnull self);


// Event queue
extern errno_t HIDManager_GetNextEvent(HIDManagerRef _Nonnull self, const struct timespec* _Nonnull timeout, HIDEvent* _Nonnull evt);
extern void HIDManager_PostEvent(HIDManagerRef _Nonnull self, HIDEventType type, did_t driverId, const HIDEventData* _Nonnull pEventData);
extern void HIDManager_FlushEvents(HIDManagerRef _Nonnull self);

#endif /* HIDManager_h */
