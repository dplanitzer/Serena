//
//  IOHIDDevice.h
//  kernel
//
//  Created by Dietmar Planitzer on 1/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef IOHIDDevice_h
#define IOHIDDevice_h

#include <driver/IODriver.h>
#include <kpi/hid.h>
#include <sched/vcpu.h>


// HID report types
typedef enum IOHIDReportType {
    kIOHIDReportType_None = 0,
    kIOHIDReportType_KeyDown,
    kIOHIDReportType_KeyUp,
    kIOHIDReportType_Mouse,
    kIOHIDReportType_GamePad,
    kIOHIDReportType_LightPen
} IOHIDReportType;


// HID report data
typedef struct IOHIDReportData_Key {
    uint16_t    keyCode;        // USB HID key scan code
} IOHIDReportData_Key;

typedef struct IOHIDReportData_Mouse {
    int16_t     dx;
    int16_t     dy;
    uint32_t    buttons;        // buttons pressed. 0: left, 1: right, 2: middle, ...
} IOHIDReportData_Mouse;

typedef struct IOHIDReportData_LightPen {
    int16_t     x;
    int16_t     y;
    uint32_t    buttons;        // buttons pressed. 0: left, 1: right, ...
    bool        hasPosition;    // true if the light pen triggered and a position could be sampled
} IOHIDReportData_LightPen;

typedef struct IOHIDReportData_GamePad {
    int16_t     x;              // (int16_t.min -> 100% left, 0 -> resting, int16_t.max -> 100% right)
    int16_t     y;              // (int16_t.min -> 100% up, 0 -> resting, int16_t.max -> 100% down)
    uint32_t    buttons;        // buttons pressed. 0: left, 1: right, ...
} IOHIDReportData_GamePad;

typedef union IOHIDReportData {
    IOHIDReportData_Key     key;
    IOHIDReportData_Mouse         mouse;
    IOHIDReportData_LightPen      lp;
    IOHIDReportData_GamePad      joy;
} IOHIDReportData;


// HID event
typedef struct IOHIDReport {
    int32_t         type;
    IOHIDReportData data;
} IOHIDReport;



// An input driver manages a specific input device and translates actions on the
// input device into events that it the posts to the HID manager.
//
open_class(IOHIDDevice, IODriver,
);
open_class_funcs(IOHIDDevice, IODriver,

    // Fills in the provided report structure with a report of the current HID
    // state of the device. If an input driver manages a queue of reports
    // internally then the override of this method should dequeue the oldest
    // queued report. Return a null report if no reports are queued. If an input
    // driver is an immediate mode driver (it does not queue reports) then the
    // override of this method should generate a report that reflects the current
    // state of the HID hardware.
    // Override: required
    // Default: returns a null report
    void (*getReport)(void* _Nonnull self, IOHIDReport* _Nonnull report);

    // Sets the kernel virtual processor that should receive signal 'signo'
    // every time the state of the HID hardware changes in the sense that the
    // state change corresponds to a new HID report. Input drivers do not generate
    // signals by default; the HID manager will call this method to enable or
    // disable signalling as needed. A driver should only generate signals when
    // the 'vp' parameter is not NULL. A NULL 'vp' parameter indicates that
    // signal generation should be disabled. A driver may not support this
    // feature (it's purely passively report only). Such a driver should return
    // ENOTSUP.
    // Override: optional
    // Default behavior: ENOTSUP
    errno_t (*setReportTarget)(void* _Nonnull self, vcpu_t _Nullable vp, int signo);
);


//
// Methods for use by the HID system.
//

#define IOHIDDevice_GetReport(__self, __report) \
invoke_n(getReport, IOHIDDevice, __self, __report)

#define IOHIDDevice_SetReportTarget(__self, __vp, __signo) \
invoke_n(setReportTarget, IOHIDDevice, __self, __vp, __signo)

#endif /* IOHIDDevice_h */
