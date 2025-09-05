//
//  kpi/hid.h
//  libc
//
//  Created by Dietmar Planitzer on 1/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_HID_H
#define _KPI_HID_H 1

#include <kpi/ioctl.h>


#define kCursor_Width  16
#define kCursor_Height 16
#define kCursor_PixelFormat    kPixelFormat_RGB_Indexed2


//
// HID Manager
//

// Dequeues and returns the next pending event from the event queue. Waits until
// an event arrives if none is pending and timeout is > 0. Returns ETIMEDOUT if
// no event has arrived before timeout. Returns EAGAIN if timeout is 0 and no
// event is pending. Note that this call disregards the O_NONBLOCK mode
// on the I/O channel.
// get_next_event(const struct timespec* _Nonnull timeout, HIDEvent* _Nonnull evt)
#define kHIDCommand_GetNextEvent IOResourceCommand(kDriverCommand_SubclassBase + 0)

// Removes all queued events from the event queue.
// flush_events(void)
#define kHIDCommand_FlushEvents IOResourceCommand(kDriverCommand_SubclassBase + 1)


// Returns the initial delay for automatic key repeats and the delay between
// successive synthesized key presses.
// get_key_repeat_delays(struct timespec* _Nullable pInitialDelay, struct timespec* _Nullable pRepeatDelay)
#define kHIDCommand_GetKeyRepeatDelays IOResourceCommand(kDriverCommand_SubclassBase + 2)

// Sets the initial delay for automatic key repeats and the delay between
// successive synthesized key presses.
// set_key_repeat_delays(const struct timespec* _Nonnull initialDelay, const struct timespec* _Nonnull repeatDelay)
#define kHIDCommand_SetKeyRepeatDelays IOResourceCommand(kDriverCommand_SubclassBase + 3)


// Set the mouse cursor image.
// set_cursor(const uint16_t* _Nonnull planes[2], int width int height, PixelFormat pixelFormat, int hotSpotX, int hotSpotY)
#define kHIDCommand_SetCursor   IOResourceCommand(kDriverCommand_SubclassBase + 4)

// Decrements the cursor hidden count and shows the cursor when the count hits 0.
//  show_cursor(void)
#define kHIDCommand_ShowCursor  IOResourceCommand(kDriverCommand_SubclassBase + 5)

// Hides the mouse cursor and increments a cursor hidden count. The cursor stays
// hidden until enough ShowCursor() calls have been made to balance the
// HideCursor() calls.
// hide_cursor(void)
#define kHIDCommand_HideCursor  IOResourceCommand(kDriverCommand_SubclassBase + 6)

// Obscures the mouse cursor. This means that the mouse cursor is temporarily
// hidden until either the user moves the mouse or you call ShowCursor(). Note
// that this function does not increment the cursor hidden count.
// int obscure_cursor()
#define kHIDCommand_ObscureCursor   IOResourceCommand(kDriverCommand_SubclassBase + 7)

// Shields the mouse cursor. Call this function before drawing into the provided
// rectangle on the screen to ensure that the mouse cursor image will be saved
// and restored as needed. This function increment the cursor hidden count. Call
// ShowCursor() to remove the shielding rectangle and to make the cursor visible
// again.
// shield_cursor(int x, int y, int width, int height)
#define kHIDCommand_ShieldCursor    IOResourceCommand(kDriverCommand_SubclassBase + 8)



//
// GamePort Controller
//

// Types of input drivers
#define IOGP_NONE               0
#define IOGP_MOUSE              1
#define IOGP_LIGHTPEN           2
#define IOGP_ANALOG_JOYSTICK    3
#define IOGP_DIGITAL_JOYSTICK   4

// Returns the type of input device for a port and the driver id of the associated
// input driver. There are two ports: 0 and 1.
// get_port_device(int port, int* _Nullable pOutType, did_t* _Nullable pOutId)
#define kGamePortCommand_GetPortDevice  IOResourceCommand(kDriverCommand_SubclassBase + 0)

// Selects the type of input device for a port. There are two port: 0 and 1.
// set_port_device(int port, int type)
#define kGamePortCommand_SetPortDevice  IOResourceCommand(kDriverCommand_SubclassBase + 1)

// Returns the number of the port to which the driver with the given driver id
// is connected. -1 is returned if the driver id doesn't refer to a driver that
// is connected to any of the game bus ports.
// get_port_for driver(did_t id, int* _Nonnull pOutPort)
#define kGamePortCommand_GetPortForDriver  IOResourceCommand(kDriverCommand_SubclassBase + 2)

#endif /* _KPI_HID_H */
