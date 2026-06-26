//
//  kpi/hid_event.h
//  kpi
//
//  Created by Dietmar Planitzer on 5/31/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_HID_EVENT_H
#define _KPI_HID_EVENT_H 1

#include <stdint.h>
#include <kpi/_time.h>
#include <kpi/types.h>


// Event types
#define HID_EVENT_KEY_DOWN      1
#define HID_EVENT_KEY_UP        2
#define HID_EVENT_FLAGS_CHANGED 3
#define HID_EVENT_MOUSE_DOWN    4
#define HID_EVENT_MOUSE_UP      5
#define HID_EVENT_MOUSE_MOVED   6
#define HID_EVENT_GPAD_DOWN     7
#define HID_EVENT_GPAD_UP       8
#define HID_EVENT_GPAD_MOTION   9


// Modifier key flags
// flags uint32_t encoding:
// [15...0]: logical modifier flags
// [23...16]: right shift / control / option / command pressed
// [31...24]: left shift / control / option / command pressed
#define HID_MODIFIER_SHIFT      1   // Any shift key except caps lock is pressed
#define HID_MODIFIER_OPTION     2   // Any option key is pressed
#define HID_MODIFIER_CONTROL    4   // Any control key is pressed
#define HID_MODIFIER_COMMAND    8   // Any command / GUI key is pressed
#define HID_MODIFIER_CAPS_LOCK  16  // Caps lock key is pressed
#define HID_MODIFIER_KEY_PAD    32  // Any key on the key pad is pressed
#define HID_MODIFIER_FUNCTION   64  // Any function key is pressed (this includes literal function 'F' keys and cursor keys, return, delete, etc)


// HID key codes are based on the USB HID key scan codes
typedef uint16_t  hid_key_t;


// HID event data
typedef struct hid_evt_key {
    uint32_t    flags;          // Modifier keys
    hid_key_t   keyCode;        // USB HID key scan code
    int         isRepeat;       // True if this is an auto-repeated key down; false otherwise
} hid_evt_key_t;


typedef struct hid_evt_flags {
    uint32_t    flags;              // Modifier keys
} hid_evt_flags_t;


typedef struct hid_evt_mouse_button {
    int         buttonNumber;   // 0 -> left button, 1 -> right button, 2-> middle button, ...
    uint32_t    flags;          // modifier keys
    int         x;              // Mouse position when the button was pressed / released
    int         y;
} hid_evt_mouse_button_t;


typedef struct hid_evt_mouse_move {
    int         x;              // Current mouse position
    int         y;
    uint32_t    flags;          // Modifier keys
} hid_evt_mouse_move_t;


typedef struct hid_evt_gpad_button {
    int         buttonNumber;
    uint32_t    flags;          // Modifier keys
    int         dx;             // Joystick direction when the button was pressed / released
    int         dy;
} hid_evt_gpad_button_t;


typedef struct hid_evt_gpad_move_t {
    int         dx;
    int         dy;
} hid_evt_gpad_move_t;


typedef union hid_event_data {
    hid_evt_key_t           key;
    hid_evt_flags_t         flags;
    hid_evt_mouse_button_t  mouse;
    hid_evt_mouse_move_t    mouseMoved;
    hid_evt_gpad_button_t   gamepad;
    hid_evt_gpad_move_t     gamepadMoved;
} hid_event_data_t;


// HID event
typedef struct hid_event {
    int32_t             type;
    did_t               driverId;   // Driver id of the driver that triggered this event; not available for keyboard and mouse events
    nanotime_t          eventTime;
    hid_event_data_t    data;
} hid_event_t;

#endif /* _KPI_HID_EVENT_H */
