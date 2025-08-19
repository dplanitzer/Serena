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
#include "HIDEventQueue.h"
#include "InputDriver.h"
#include <kpi/hidkeycodes.h>
#include <machine/amiga/graphics/GraphicsDriver.h>
#include <machine/irq.h>
#include <sched/mtx.h>
#include <sched/vcpu.h>


#define SIGKEY  2
#define SIGVBL  3


// XXX 16 is confirmed to work without overflows on a A2000. Still want to keep
// 48 for now for mouse move. Though once we support coalescing we may want to
// revisit this.
#define REPORT_QUEUE_MAX_EVENTS 48
#define MAX_GAME_PADS           2
#define MAX_POINTING_DEVICES    2
#define KEY_MAP_INTS_COUNT      (256/32)


// State of the logical pointing device (mouse)
typedef struct logical_mouse {
    size_t                  chCount;
    IOChannelRef _Nullable  ch[MAX_POINTING_DEVICES];
    int16_t                 x;
    int16_t                 y;
    uint32_t                buttons;
    MouseCursorVisibility   visibility;
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
    GraphicsDriverRef _Nullable fb;


    // HID reports collector
    vcpu_t _Nullable            reportsCollector;
    struct waitqueue            reportsWaitQueue;
    sigset_t                    reportSigs;
    HIDReport                   report;
    irq_handler_t               vblHandler;


    // Event queue
    HIDEventQueueRef _Nonnull   eventQueue;


    // Keyboard Configuration
    const uint8_t*              keyFlags;


    // Mouse Configuration
    int16_t                     screenLeft;
    int16_t                     screenTop;
    int16_t                     screenRight;
    int16_t                     screenBottom;
    int16_t                     shieldingLeft;
    int16_t                     shieldingTop;
    int16_t                     shieldingRight;
    int16_t                     shieldingBottom;
    int16_t                     hotSpotX;
    int16_t                     hotSpotY;
    bool                        isMouseShieldActive;
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

#endif /* HIDManagerPriv_h */
