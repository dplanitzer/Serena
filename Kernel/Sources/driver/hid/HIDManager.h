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
#include <System/Error.h>
#include <sys/fb.h>
#include <sys/hid.h>
#include <sys/hidevent.h>


extern HIDManagerRef _Nonnull gHIDManager;

extern errno_t HIDManager_Create(HIDManagerRef _Nullable * _Nonnull pOutSelf);
extern errno_t HIDManager_Start(HIDManagerRef _Nonnull self);


// APIs for use by input drivers
extern void HIDManager_ReportKeyboardDeviceChange(HIDManagerRef _Nonnull self, HIDKeyState keyState, uint16_t keyCode);
extern void HIDManager_ReportMouseDeviceChange(HIDManagerRef _Nonnull self, int16_t xDelta, int16_t yDelta, uint32_t buttonsDown);
extern void HIDManager_ReportJoystickDeviceChange(HIDManagerRef _Nonnull self, int port, int16_t xAbs, int16_t yAbs, uint32_t buttonsDown);
extern void HIDManager_ReportLightPenDeviceChange(HIDManagerRef _Nonnull self, int16_t xAbs, int16_t yAbs, bool hasPosition, uint32_t buttonsDown);
extern bool HIDManager_GetLightPenPosition(HIDManagerRef _Nonnull self, int16_t* _Nonnull pPosX, int16_t* _Nonnull pPosY);


// Configuring the keyboard
extern void HIDManager_GetKeyRepeatDelays(HIDManagerRef _Nonnull self, struct timespec* _Nullable pInitialDelay, struct timespec* _Nullable pRepeatDelay);
extern void HIDManager_SetKeyRepeatDelays(HIDManagerRef _Nonnull self, struct timespec initialDelay, struct timespec repeatDelay);


// Returns the keyboard hardware state
extern void HIDManager_GetDeviceKeysDown(HIDManagerRef _Nonnull self, const HIDKeyCode* _Nullable pKeysToCheck, int nKeysToCheck, HIDKeyCode* _Nullable pKeysDown, int* _Nonnull nKeysDown);


// GamePort control
extern errno_t HIDManager_GetPortDevice(HIDManagerRef _Nonnull self, int port, InputType* _Nullable pOutType);
extern errno_t HIDManager_SetPortDevice(HIDManagerRef _Nonnull self, int port, InputType type);


// Mouse cursor state
extern errno_t HIDManager_SetMouseCursor(HIDManagerRef _Nonnull self, const uint16_t* _Nullable planes[2], int width, int height, PixelFormat pixelFormat, int hotSpotX, int hotSpotY);
extern errno_t HIDManager_SetMouseCursorVisibility(HIDManagerRef _Nonnull self, MouseCursorVisibility mode);
extern MouseCursorVisibility HIDManager_GetMouseCursorVisibility(HIDManagerRef _Nonnull self);
extern errno_t HIDManager_ShieldMouseCursor(HIDManagerRef _Nonnull self, int x, int y, int width, int height);
extern void HIDManager_UnshieldMouseCursor(HIDManagerRef _Nonnull self);


// Returns the mouse hardware state
extern void HIDManager_GetMouseDevicePosition(HIDManagerRef _Nonnull self, int* _Nonnull pOutX, int* _Nonnull pOutY);
extern uint32_t HIDManager_GetMouseDeviceButtonsDown(HIDManagerRef _Nonnull self);


extern errno_t HIDManager_GetNextEvent(HIDManagerRef _Nonnull self, struct timespec timeout, HIDEvent* _Nonnull evt);

#endif /* HIDManager_h */
