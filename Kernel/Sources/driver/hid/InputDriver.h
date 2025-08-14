//
//  InputDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 1/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef InputDriver_h
#define InputDriver_h

#include <driver/Driver.h>
#include <kpi/hid.h>
#include <sched/vcpu.h>


// HID report types
typedef enum HIDReportType {
    kHIDReportType_Null = 0,
    kHIDReportType_KeyDown,
    kHIDReportType_KeyUp,
    kHIDReportType_Mouse,
    kHIDReportType_Joystick,
    kHIDReportType_LightPen
} HIDReportType;


// HID report data
typedef struct HIDReportData_KeyUpDown {
    uint16_t    keyCode;        // USB HID key scan code
} HIDReportData_KeyUpDown;

typedef struct HIDReportData_Mouse {
    int16_t     dx;
    int16_t     dy;
    uint32_t    buttons;        // buttons pressed. 0: left, 1: right, 2: middle, ...
} HIDReportData_Mouse;

typedef struct HIDReportData_LightPen {
    int16_t     x;
    int16_t     y;
    uint32_t    buttons;        // buttons pressed. 0: left, 1: right, ...
    bool        hasPosition;    // true if the light pen triggered and a position could be sampled
} HIDReportData_LightPen;

typedef struct HIDReportData_Joystick {
    int16_t     x;              // (int16_t.min -> 100% left, 0 -> resting, int16_t.max -> 100% right)
    int16_t     y;              // (int16_t.min -> 100% up, 0 -> resting, int16_t.max -> 100% down)
    uint32_t    buttons;        // buttons pressed. 0: left, 1: right, ...
} HIDReportData_Joystick;

typedef union _HIDReportData {
    HIDReportData_KeyUpDown     key;
    HIDReportData_Mouse         mouse;
    HIDReportData_LightPen      lp;
    HIDReportData_Joystick      joy;
} HIDReportData;


// HID event
typedef struct HIDReport {
    int32_t         type;
    HIDReportData   data;
} HIDReport;



// An input driver manages a specific input device and translates actions on the
// input device into events that it the posts to the HID manager.
//
open_class(InputDriver, Driver,
);
open_class_funcs(InputDriver, Driver,

    // Returns information about the input device.
    // Override: optional
    // Default Behavior: returns info for a null input device
    void (*getInfo)(void* _Nonnull _Locked self, InputInfo* _Nonnull pOutInfo);

    // Returns the input driver type.
    // Override: required
    // Default Behavior: returns kInputType_None
    InputType (*getInputType)(void* _Nonnull _Locked self);


    // Fills in the provided report structure with a report of the current HID
    // state of the device. If an input driver manages a queue of reports
    // internally then the override of this method should dequeue the oldest
    // queued report. Return a null report if no reports are queued. If an input
    // driver is an immediate mode driver (it does not queue reports) then the
    // override of this method should generate a report that reflects the current
    // state of the HID hardware.
    // Override: required
    // Default Behavior: returns a null report
    void (*getReport)(void* _Nonnull self, HIDReport* _Nonnull report);

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

#define InputDriver_GetReport(__self, __report) \
invoke_n(getReport, InputDriver, __self, __report)

#define InputDriver_SetReportTarget(__self, __vp, __signo) \
invoke_n(setReportTarget, InputDriver, __self, __vp, __signo)



//
// Subclassers
//

#define InputDriver_GetInputType(__self) \
((InputType) invoke_0(getInputType, InputDriver, __self))

#define InputDriver_GetInfo(__self, __info) \
invoke_n(getInfo, InputDriver, __self, __info)


#endif /* InputDriver_h */
