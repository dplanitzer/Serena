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
#include "HIDEvent.h"
#include <System/HID.h>


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
extern void HIDManager_GetKeyRepeatDelays(HIDManagerRef _Nonnull self, TimeInterval* _Nullable pInitialDelay, TimeInterval* _Nullable pRepeatDelay);
extern void HIDManager_SetKeyRepeatDelays(HIDManagerRef _Nonnull self, TimeInterval initialDelay, TimeInterval repeatDelay);


// Returns the keyboard hardware state
extern void HIDManager_GetDeviceKeysDown(HIDManagerRef _Nonnull self, const HIDKeyCode* _Nullable pKeysToCheck, int nKeysToCheck, HIDKeyCode* _Nullable pKeysDown, int* _Nonnull nKeysDown);


// GamePort control
extern errno_t HIDManager_GetPortDevice(HIDManagerRef _Nonnull self, int port, InputType* _Nullable pOutType);
extern errno_t HIDManager_SetPortDevice(HIDManagerRef _Nonnull self, int port, InputType type);


// The mouse cursor state
extern void HIDManager_SetMouseCursor(HIDManagerRef _Nonnull self, const void* pBitmap, const void* pMask);
extern void HIDManager_ShowMouseCursor(HIDManagerRef _Nonnull self);
extern void HIDManager_HideMouseCursor(HIDManagerRef _Nonnull self);
extern void HIDManager_SetMouseCursorHiddenUntilMouseMoves(HIDManagerRef _Nonnull self, bool flag);

// Returns the mouse hardware state
extern Point HIDManager_GetMouseDevicePosition(HIDManagerRef _Nonnull self);
extern uint32_t HIDManager_GetMouseDeviceButtonsDown(HIDManagerRef _Nonnull self);


// Returns events in the order oldest to newest. As many events are returned as
// fit in the provided buffer. Only blocks the caller if no events are queued.
extern errno_t HIDManager_GetEvents(HIDManagerRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, TimeInterval timeout, ssize_t* _Nonnull nOutBytesRead);

#endif /* HIDManager_h */
