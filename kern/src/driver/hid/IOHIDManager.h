//
//  IOHIDManager.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef IOHIDManager_h
#define IOHIDManager_h

#include <kobj/Object.h>
#include <ext/try.h>
#include <kobj/Object.h>
#include <kpi/framebuffer.h>
#include <kpi/hid.h>
#include <kpi/hid_event.h>


final_class(IOHIDManager, Object);

extern IOHIDManagerRef gIOHIDManager;


extern errno_t IOHIDManager_Create(IOHIDManagerRef _Nullable * _Nonnull pOutSelf);
extern errno_t IOHIDManager_Start(IOHIDManagerRef _Nonnull self);


// Configuring the keyboard
extern void IOHIDManager_GetKeyRepeatDelays(IOHIDManagerRef _Nonnull self, nanotime_t* _Nullable pInitialDelay, nanotime_t* _Nullable pRepeatDelay);
extern void IOHIDManager_SetKeyRepeatDelays(IOHIDManagerRef _Nonnull self, const nanotime_t* _Nonnull initialDelay, const nanotime_t* _Nonnull repeatDelay);


// Returns the keyboard hardware state
extern void IOHIDManager_GetDeviceKeysDown(IOHIDManagerRef _Nonnull self, const hid_key_t* _Nullable pKeysToCheck, int nKeysToCheck, hid_key_t* _Nullable pKeysDown, int* _Nonnull nKeysDown);


// Mouse cursor state
extern errno_t IOHIDManager_ObtainCursor(IOHIDManagerRef _Nonnull self);
extern void IOHIDManager_ReleaseCursor(IOHIDManagerRef _Nonnull self);
extern errno_t IOHIDManager_SetCursor(IOHIDManagerRef _Nonnull self, const void* _Nullable planes[], size_t bytesPerRow, int width, int height, vio_pixfmt_t format, int hotSpotX, int hotSpotY);
extern void IOHIDManager_ShowCursor(IOHIDManagerRef _Nonnull self);
extern void IOHIDManager_HideCursor(IOHIDManagerRef _Nonnull self);
extern void IOHIDManager_ObscureCursor(IOHIDManagerRef _Nonnull self);
extern errno_t IOHIDManager_ShieldMouseCursor(IOHIDManagerRef _Nonnull self, int x, int y, int width, int height);


// Returns the mouse hardware state
extern void IOHIDManager_GetMouseDevicePosition(IOHIDManagerRef _Nonnull self, int* _Nonnull pOutX, int* _Nonnull pOutY);
extern uint32_t IOHIDManager_GetMouseDeviceButtonsDown(IOHIDManagerRef _Nonnull self);


// Event queue
extern errno_t IOHIDManager_GetNextEvent(IOHIDManagerRef _Nonnull self, const nanotime_t* _Nonnull timeout, hid_event_t* _Nonnull evt);
extern void IOHIDManager_PostEvent(IOHIDManagerRef _Nonnull self, int type, did_t driverId, const hid_event_data_t* _Nonnull pEventData);
extern void IOHIDManager_FlushEvents(IOHIDManagerRef _Nonnull self);


// Game port bus control
#if __IOGPBUS__ > 0
extern size_t IOHIDManager_GetPortCount(IOHIDManagerRef _Nonnull self);
extern errno_t IOHIDManager_GetPortDevice(IOHIDManagerRef _Nonnull self, int port, int* _Nullable pOutType, did_t* _Nullable pOutId);
extern errno_t IOHIDManager_SetPortDevice(IOHIDManagerRef _Nonnull self, int port, int type);
extern errno_t IOHIDManager_GetPortForDeviceId(IOHIDManagerRef _Nonnull self, did_t id, int* _Nonnull pOutPort);
#endif

#endif /* IOHIDManager_h */
