//
//  hid_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 1/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/hid.h>
#include <sys/hidevent.h>
#include <sys/hidkeycodes.h>
#include <sys/ioctl.h>
#include <sys/timespec.h>
#include "Asserts.h"


void hid_test(int argc, char *argv[])
{
    const int fd = open("/dev/hid", O_RDONLY);
    HIDEvent evt;
    bool done = false;

    assertGreaterEqual(0, fd);
    printf("Press 'q' to quit.\n");

    while (!done) {
        const int r = ioctl(fd, kHIDCommand_GetNextEvent, &TIMESPEC_INF, &evt);
        
        assertGreaterEqual(0, r);
        switch (evt.type) {
            case kHIDEventType_KeyDown:
            case kHIDEventType_KeyUp:
                printf("%s: $%hhx\tflags: $%hhx\tisRepeat: %s\n",
                      (evt.type == kHIDEventType_KeyUp) ? "key-up" : "key-down",
                      (int)evt.data.key.keyCode,
                      (int)evt.data.key.flags, evt.data.key.isRepeat ? "true" : "false");

                if (evt.type == kHIDEventType_KeyDown && evt.data.key.keyCode == KEY_Q) {
                    done = true;
                }
                break;
                
            case kHIDEventType_FlagsChanged:
                printf("flags-changed: $%hhx\n", evt.data.flags.flags);
                break;
                
            case kHIDEventType_MouseUp:
            case kHIDEventType_MouseDown:
                printf("%s: %d\tflags: $%hhx\t(%d, %d)\n",
                      (evt.type == kHIDEventType_MouseUp) ? "mouse-up" : "mouse-down",
                      evt.data.mouse.buttonNumber,
                      evt.data.mouse.x,
                      evt.data.mouse.y,
                      (int)evt.data.mouse.flags);
                break;

            case kHIDEventType_MouseMoved:
                printf("mouse-moved\t(%d, %d)\n",
                      evt.data.mouseMoved.x,
                      evt.data.mouseMoved.y);
                break;

            case kHIDEventType_JoystickUp:
            case kHIDEventType_JoystickDown:
                printf("%s: %d\tflags: $%hhx\t(%d, %d)\n",
                      (evt.type == kHIDEventType_JoystickUp) ? "joy-up" : "joy-down",
                      evt.data.joystick.buttonNumber,
                      evt.data.joystick.dx,
                      evt.data.joystick.dy,
                      (int)evt.data.joystick.flags);
                break;

            case kHIDEventType_JoystickMotion:
                printf("joy-motion\t(%d, %d)\n",
                      evt.data.joystickMotion.dx,
                      evt.data.joystickMotion.dy);
                break;

            default:
                printf("*** unknown\n");
        }
    }

    close(fd);
}
