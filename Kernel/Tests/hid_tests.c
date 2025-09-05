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
#include <sys/fb.h>
#include <sys/hid.h>
#include <sys/hidevent.h>
#include <sys/hidkeycodes.h>
#include <sys/ioctl.h>
#include <sys/timespec.h>
#include "Asserts.h"

#define PACK_U16(_15, _14, _13, _12, _11, _10, _9, _8, _7, _6, _5, _4, _3, _2, _1, _0) \
    (uint16_t)(((_15) << 15) | ((_14) << 14) | ((_13) << 13) | ((_12) << 12) | ((_11) << 11) |\
             ((_10) << 10) |  ((_9) <<  9) |  ((_8) <<  8) |  ((_7) <<  7) |  ((_6) <<  6) |\
              ((_5) <<  5) |  ((_4) <<  4) |  ((_3) <<  3) |  ((_2) <<  2) |  ((_1) <<  1) | (_0))

#define _ 0
#define o 1

#define ARROW_BITS_PLANE0 \
PACK_U16( o,o,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,o,o,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,o,o,o,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,o,o,o,o,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,_,o,o,o,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,_,_,o,o,o,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,_,_,_,_,o,o,o,o,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,o,o,o,o,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,o,o,o,_,_,_,_,_,_,_ ),
static const uint16_t gArrow_Plane0[] = {
    ARROW_BITS_PLANE0
};

#define ARROW_BITS_PLANE1 \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,o,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,o,o,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,o,o,o,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,o,o,o,o,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,o,o,o,o,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,o,o,o,o,o,o,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,o,o,o,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,_,o,o,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,_,_,_,o,o,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,o,o,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,o,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,o,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ),
static const uint16_t gArrow_Plane1[] = {
    ARROW_BITS_PLANE1
};

static const uint16_t* gArrow_Planes[2] = {
    gArrow_Plane0,
    gArrow_Plane1
};

static const int gArrow_Width = 16;
static const int gArrow_Height = 16;



void hid_test(int argc, char *argv[])
{
    HIDEvent evt;
    bool mc_vis = true;
    bool done = false;

    const int fd = open("/dev/hid", O_RDONLY);
    assertGreaterEqual(0, fd);
    printf("Press '1' to toggle mouse cursor visibility.\n");
    printf("Press '2' to hide mouse cursor until move.\n");
    printf("Press 'q' to quit.\n");

    //const uint16_t* _Nonnull planes[2], int width int height, PixelFormat pixelFormat, int hotSpotX, int hotSpotY
    assertGreaterEqual(0, ioctl(fd, kHIDCommand_SetCursor, gArrow_Planes, gArrow_Width, gArrow_Height, kCursor_PixelFormat, 0, 0));
    assertGreaterEqual(0, ioctl(fd, kHIDCommand_ShowCursor));

    while (!done) {
        assertGreaterEqual(0, ioctl(fd, kHIDCommand_GetNextEvent, &TIMESPEC_INF, &evt));

        switch (evt.type) {
            case kHIDEventType_KeyDown:
            case kHIDEventType_KeyUp:
                printf("%s: $%hhx\tflags: $%hhx\tisRepeat: %s\n",
                      (evt.type == kHIDEventType_KeyUp) ? "key-up" : "key-down",
                      (int)evt.data.key.keyCode,
                      (int)evt.data.key.flags, evt.data.key.isRepeat ? "true" : "false");

                if (evt.type == kHIDEventType_KeyDown) {
                    switch (evt.data.key.keyCode) {
                        case KEY_Q:
                            done = true;
                            break;

                        case KEY_1:
                            if (mc_vis) {
                                assertGreaterEqual(0, ioctl(fd, kHIDCommand_HideCursor));
                                mc_vis = false;
                            }
                            else {
                                assertGreaterEqual(0, ioctl(fd, kHIDCommand_ShowCursor));
                                mc_vis = true;
                            }
                            break;

                        case KEY_2:
                            assertGreaterEqual(0, ioctl(fd, kHIDCommand_ObscureCursor));
                            break;

                        default:
                            break;
                    }
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
                      (int)evt.data.mouse.flags,
                      evt.data.mouse.x,
                      evt.data.mouse.y);
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
                      (int)evt.data.joystick.flags,
                      evt.data.joystick.dx,
                      evt.data.joystick.dy);
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

    assertGreaterEqual(0, ioctl(fd, kHIDCommand_FlushEvents));
    assertGreaterEqual(0, ioctl(fd, kHIDCommand_HideCursor));
    close(fd);
}
