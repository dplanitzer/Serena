//
//  kpi/hid.h
//  kpi
//
//  Created by Dietmar Planitzer on 1/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_HID_H
#define _KPI_HID_H 1

#include <kpi/ioctl.h>


#define HID_CURSOR_WIDTH    16
#define HID_CURSOR_HEIGHT   16
#define HID_CURSOR_PIXELFORMAT  GD_COLOR_INDEX2


//
// HID Manager
//

// Dequeues and returns the next pending event from the event queue. Waits until
// an event arrives if none is pending and timeout is > 0. Returns ETIMEDOUT if
// no event has arrived before timeout. Returns EAGAIN if timeout is 0 and no
// event is pending. Note that this call disregards the O_NONBLOCK mode
// on the I/O channel.
// get_event(const nanotime_t* _Nonnull timeout, hid_event_t* _Nonnull evt)
#define HID_CMD_GET_EVENT \
IOCMD_MAKE(IOPROTO_HID, 1, _IOCMD_ACC_RD, 0)

// Removes all queued events from the event queue.
// flush_events(void)
#define HID_CMD_FLUSH_EVENTS \
IOCMD_MAKE(IOPROTO_HID, 2, _IOCMD_ACC_WR, 0)


// Returns the initial delay for automatic key repeats and the delay between
// successive synthesized key presses.
// get_key_repeat_delays(nanotime_t* _Nullable pInitialDelay, nanotime_t* _Nullable pRepeatDelay)
#define HID_CMD_KEY_REPEAT_DELAYS \
IOCMD_MAKE(IOPROTO_HID, 3, _IOCMD_ACC_RD, 0)

// Sets the initial delay for automatic key repeats and the delay between
// successive synthesized key presses.
// set_key_repeat_delays(const nanotime_t* _Nonnull initialDelay, const nanotime_t* _Nonnull repeatDelay)
#define HID_CMD_SET_KEY_REPEAT_DELAYS \
IOCMD_MAKE(IOPROTO_HID, 4, _IOCMD_ACC_WR, 0)


// Obtains the mouse cursor. The mouse cursor is initially transparent and thus
// nothing will show up initially on the screen. You must set a cursor
// image by calling SetCursor() before anything will show up on the screen.
// Returns EOK on success and a suitable error if the cursor services can not
// be started because eg the necessary resources are not currently available. 
// obtain_cursor(void)
#define HID_CMD_OBTAIN_CURSOR \
IOCMD_MAKE(IOPROTO_HID, 5, _IOCMD_ACC_WR, 0)

// Releases the mouse cursor and makes the underlying sprite available for
// other uses.
// relinquish_cursor(void)
#define HID_CMD_RELEASE_CURSOR \
IOCMD_MAKE(IOPROTO_HID, 6, _IOCMD_ACC_WR, 0)

// Sets the mouse cursor image to the provided pixels. Note that only some pixel
// format are supported:
// GD_COLOR_INDEX2
// Color index #0 is interpreted as transparent for all indexed pixel formats.
// set_cursor(const void* _Nonnull planes[], size_t bytesPerRow, int width, int height, gd_pixfmt_t format, int hotSpotX, int hotSpotY)
#define HID_CMD_SET_CURSOR \
IOCMD_MAKE(IOPROTO_HID, 7, _IOCMD_ACC_WR, 0)

// Decrements the cursor hidden count and shows the cursor when the count hits 0.
//  show_cursor(void)
#define HID_CMD_SHOW_CURSOR \
IOCMD_MAKE(IOPROTO_HID, 8, _IOCMD_ACC_WR, 0)

// Hides the mouse cursor and increments a cursor hidden count. The cursor stays
// hidden until enough ShowCursor() calls have been made to balance the
// HideCursor() calls.
// hide_cursor(void)
#define HID_CMD_HIDE_CURSOR \
IOCMD_MAKE(IOPROTO_HID, 9, _IOCMD_ACC_WR, 0)

// Obscures the mouse cursor. This means that the mouse cursor is temporarily
// hidden until either the user moves the mouse or you call ShowCursor(). Note
// that this function does not increment the cursor hidden count.
// int obscure_cursor()
#define HID_CMD_OBSCURE_CURSOR \
IOCMD_MAKE(IOPROTO_HID, 10, _IOCMD_ACC_WR, 0)

// Shields the mouse cursor. Call this function before drawing into the provided
// rectangle on the screen to ensure that the mouse cursor image will be saved
// and restored as needed. This function increment the cursor hidden count. Call
// ShowCursor() to remove the shielding rectangle and to make the cursor visible
// again.
// shield_cursor(int x, int y, int width, int height)
#define HID_CMD_SHIELD_CURSOR \
IOCMD_MAKE(IOPROTO_HID, 11, _IOCMD_ACC_WR, 0)



//
// Game Port Bus Support (Platform Specific)
//

// Types of HID devices supported by the game port bus
#define HID_PORT_NONE       0
#define HID_PORT_MOUSE      1
#define HID_PORT_LIGHT_PEN  2
#define HID_PORT_PADDLE     3
#define HID_PORT_JOYSTICK   4

// Returns the number of ports supported by the game port bus.
// get_port_count(size_t* _Nonnull count)
#define HID_CMD_PORT_COUNT \
IOCMD_MAKE(IOPROTO_HID, 12, _IOCMD_ACC_RD, 0)

// Returns the type of input device for a port and the driver id of the associated
// input driver. There are two ports: 0 and 1.
// get_port_device(int port, int* _Nullable pOutType, did_t* _Nullable pOutId)
#define HID_CMD_PORT_DEVICE \
IOCMD_MAKE(IOPROTO_HID, 13, _IOCMD_ACC_RD, 0)

// Selects the type of input device for a port. There are two port: 0 and 1.
// set_port_device(int port, int type)
#define HID_CMD_SET_PORT_DEVICE \
IOCMD_MAKE(IOPROTO_HID, 14, _IOCMD_ACC_WR, 0)

// Returns the number of the port to which the driver with the given driver id
// is connected. -1 is returned if the driver id doesn't refer to a driver that
// is connected to any of the game bus ports.
// get_port_for_device_id(did_t id, int* _Nonnull pOutPort)
#define HID_CMD_PORT_FOR_DEVICE \
IOCMD_MAKE(IOPROTO_HID, 15, _IOCMD_ACC_RD, 0)

#endif /* _KPI_HID_H */
