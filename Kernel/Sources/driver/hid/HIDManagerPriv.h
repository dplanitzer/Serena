//
//  HIDManagerPriv.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/04/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef HIDManagerPriv_h
#define HIDManagerPriv_h

#include "HIDManager.h"
#include "HIDEventSynth.h"
#include "InputDriver.h"
#include <driver/DisplayDriver.h>
#include <hal/clock.h>
#include <hal/irq.h>
#include <kpi/hidkeycodes.h>
#include <sched/cnd.h>
#include <sched/mtx.h>
#include <sched/vcpu.h>


#define SIGVBL  SIGUSR1
#define SIGSCR  SIGUSR2


// XXX 16 is confirmed to work without overflows on a A2000. Still want to keep
// 48 for now for mouse move. Though once we support coalescing we may want to
// revisit this.
#define REPORT_QUEUE_MAX_EVENTS 48
#define MAX_GAME_PADS           2
#define MAX_POINTING_DEVICES    2
#define KEY_MAP_INTS_COUNT      (256/32)


typedef struct hid_rect {
    int16_t     l;
    int16_t     t;
    int16_t     b;
    int16_t     r;
} hid_rect_t;


// State of the logical pointing device (mouse)
typedef struct logical_mouse {
    IOChannelRef _Nullable  ch[MAX_POINTING_DEVICES];
    int16_t                 chCount;
    int16_t                 lpCount;
    int16_t                 x;
    int16_t                 y;
    uint32_t                buttons;
} logical_mouse_t;


// State of a gamepad/joystick device
typedef struct gamepad_state {
    IOChannelRef _Nullable  ch;
    int16_t     x;        // int16_t.min -> 100% left, 0 -> resting, int16_t.max -> 100% right
    int16_t     y;        // int16_t.min -> 100% up, 0 -> resting, int16_t.max -> 100% down
    uint32_t    buttons;  // Button #0 -> 0, Button #1 -> 1, ...
} gamepad_state_t;


// Explanation of logical keyboard/mouse device:
//
// The event driver maintains a logical keyboard and mouse device. These devices
// reflect the current state of the hardware as closely as possible and with as
// little latency as possible. So this state is maintained before the event queue.
// However these devices are logical in the sense that multiple hardware devices
// may contribute to their state. Eg multiple keyboards may contribute to the
// logical keyboard and multiple mice and other devices such as a joystick or
// light pen may contribute to the state of the logical mouse.
typedef struct HIDManager {
    mtx_t                       mtx;


    // Input Drivers
    IOChannelRef _Nullable      kbChannel;
    InputDriverRef _Nullable    kb;


    // Framebuffer interface
    IOChannelRef _Nullable      fbChannel;
    DisplayDriverRef _Nullable  fb;


    // HID reports collector
    vcpu_t _Nullable            reportsCollector;
    struct waitqueue            reportsWaitQueue;
    sigset_t                    reportSigs;
    HIDReport                   report;
    struct timespec             now;        // Current time from the viewpoint of the reports collector
    irq_handler_t               vblHandler;


    // Event queue
    cnd_t                       evqCnd;
    HIDEventSynth               evqSynth;
    HIDSynthResult              evqSynthResult;
    uint16_t                    evqCapacity;
    uint16_t                    evqCapacityMask;
    uint16_t                    evqReadIdx;
    uint16_t                    evqWriteIdx;
    size_t                      evqOverflowCount;
    HIDEvent* _Nonnull          evqQueue;


    // Keyboard Configuration
    const uint8_t*              keyFlags;


    // Mouse Configuration and Mouse Cursor
    hid_rect_t                  screenBounds;
    hid_rect_t                  shieldRect;
    hid_rect_t                  cursorBounds;   // updated only when needed
    int                         cursorSurfaceId;
    int16_t                     cursorWidth;
    int16_t                     cursorHeight;
    int16_t                     hotSpotX;
    int16_t                     hotSpotY;
    int                         hiddenCount;
    bool                        isMouseObscured;
    bool                        isMouseShielded;
    bool                        isMouseShieldEnabled;
    bool                        isMouseMoveReportingEnabled;    // true if position-change-only mouse reports should be queued; false if we only care about mouse button changes


    // Logical Keyboard Device
    //
    // XXX A word on the key map:
    // The embedded map here is just a note for the future. In the future the map
    // will be in a sharable page. Apps will then be able to map that page read-only
    // via an iocall. The app will then be able to copy/scan the map as needed while
    // the input keyboard driver updates it.
    uint32_t                    keyMap[KEY_MAP_INTS_COUNT];    // keycode is the bit index. 1 -> key down; 0 -> key up
    uint32_t                    modifierFlags;


    // Logical Mouse Device
    logical_mouse_t             mouse;


    // Gamepad style devices
    size_t                      gamepadCount;
    gamepad_state_t             gamepad[MAX_GAME_PADS];
} HIDManager;


#define hid_rect_intersection(__a, __b, __r) \
(__r)->l = __max((__a)->l, (__b)->l); \
(__r)->t = __max((__a)->t, (__b)->t); \
(__r)->r = __min((__a)->r, (__b)->r); \
(__r)->b = __min((__a)->b, (__b)->b)

#define hid_rect_intersects(__a, __b, __r) \
{ \
const int16_t __x0 = __max((__a)->l, (__b)->l); \
const int16_t __y0 = __max((__a)->t, (__b)->t); \
const int16_t __x1 = __min((__a)->r, (__b)->r); \
const int16_t __y1 = __min((__a)->b, (__b)->b); \
(__r) = (__x1 - __x0) > 0 && (__y1 - __y0) > 0; \
}

#define hid_rect_contains(__r, __x, __y) \
(((__x) >= (__r)->l && (__x) < (__r)->r && (__y) >= (__r)->t && (__y) < (__r)->b) ? true : false)

#define hid_rect_set_empty(__r) \
(__r)->l = 0; (__r)->t = 0; (__r)->b = 0; (__r)->r = 0


// Returns the number of events stored in the ring queue - aka the number of
// events that can be read from the queue.
#define EVQ_READABLE_COUNT() \
(self->evqWriteIdx - self->evqReadIdx)

// Returns the number of events that can be written to the queue.
#define EVQ_WRITABLE_COUNT() \
(self->evqCapacity - (self->evqWriteIdx - self->evqReadIdx))

#endif /* HIDManagerPriv_h */
