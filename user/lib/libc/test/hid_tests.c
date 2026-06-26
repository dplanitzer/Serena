//
//  hid_tests.c
//  C Tests
//
//  Created by Dietmar Planitzer on 1/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ext/nanotime.h>
#include <serena/framebuffer.h>
#include <serena/fd.h>
#include <serena/file.h>
#include <serena/hid.h>
#include <serena/hid_event.h>
#include <serena/hid_keys.h>
#include "asserts.h"

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



static const char* _hid_device_type_string(int type)
{
    switch (type) {
        case HID_PORT_NONE:         return "none";
        case HID_PORT_MOUSE:        return "mouse";
        case HID_PORT_LIGHT_PEN:    return "light pen";
        case HID_PORT_PADDLE:       return "paddle";
        case HID_PORT_JOYSTICK:     return "joystick";
        default:                    return "??";
    }
}

void hid_test(int argc, char *argv[])
{
    hid_event_t evt;
    bool mc_vis = true;
    bool done = false;
    size_t gp_count = 0;

    const int fd = fs_open(NULL, "/dev/hid", O_RDONLY);
    assert_int_ge(0, fd);


    // Game port
    assert_int_ge(0, fd_cntl(fd, IOCMD_HID_PORT_COUNT, &gp_count));
    printf("\nGame Ports: %d\n", gp_count);

    for (size_t i = 0; i < gp_count; i++) {
        int type;
        did_t device_id;

        assert_int_ge(0, fd_cntl(fd, IOCMD_HID_PORT_DEVICE, i, &type, &device_id));
        printf("  #%d: %s [%u]\n", i, _hid_device_type_string(type), device_id);
    }
    printf("\n\n");


    // Event loop
    printf("Press '1' to toggle mouse cursor visibility.\n");
    printf("Press '2' to hide mouse cursor until move.\n");
    printf("Press 'q' to quit.\n\n");

    assert_int_ge(0, fd_cntl(fd, IOCMD_HID_OBTAIN_CURSOR));
    assert_int_ge(0, fd_cntl(fd, IOCMD_HID_SET_CURSOR, gArrow_Planes, sizeof(uint16_t), kCursor_Width, kCursor_Height, kCursor_PixelFormat, 1, 1));

    while (!done) {
        assert_int_ge(0, fd_cntl(fd, IOCMD_HID_GET_EVENT, &NANOTIME_INF, &evt));

        switch (evt.type) {
            case HID_EVENT_KEY_DOWN:
            case HID_EVENT_KEY_UP:
                printf("%s: $%hhx\tflags: $%hhx\tisRepeat: %s\n",
                      (evt.type == HID_EVENT_KEY_UP) ? "key-up" : "key-down",
                      (int)evt.data.key.keyCode,
                      (int)evt.data.key.flags, evt.data.key.isRepeat ? "true" : "false");

                if (evt.type == HID_EVENT_KEY_DOWN) {
                    switch (evt.data.key.keyCode) {
                        case KEY_Q:
                            done = true;
                            break;

                        case KEY_1:
                            if (mc_vis) {
                                assert_int_ge(0, fd_cntl(fd, IOCMD_HID_HIDE_CURSOR));
                                mc_vis = false;
                            }
                            else {
                                assert_int_ge(0, fd_cntl(fd, IOCMD_HID_SHOW_CURSOR));
                                mc_vis = true;
                            }
                            break;

                        case KEY_2:
                            assert_int_ge(0, fd_cntl(fd, IOCMD_HID_OBSCURE_CURSOR));
                            break;

                        default:
                            break;
                    }
                }
                break;
                
            case HID_EVENT_FLAGS_CHANGED:
                printf("flags-changed: $%hhx\n", evt.data.flags.flags);
                break;
                
            case HID_EVENT_MOUSE_UP:
            case HID_EVENT_MOUSE_DOWN:
                printf("%s: %d\tflags: $%hhx\t(%d, %d)\n",
                      (evt.type == HID_EVENT_MOUSE_UP) ? "mouse-up" : "mouse-down",
                      evt.data.mouse.buttonNumber,
                      (int)evt.data.mouse.flags,
                      evt.data.mouse.x,
                      evt.data.mouse.y);
                break;

            case HID_EVENT_MOUSE_MOVED:
                printf("mouse-moved\t(%d, %d)\n",
                      evt.data.mouseMoved.x,
                      evt.data.mouseMoved.y);
                break;

            case HID_EVENT_GPAD_UP:
            case HID_EVENT_GPAD_DOWN:
                printf("%s: %d\tflags: $%hhx\t(%d, %d)\n",
                      (evt.type == HID_EVENT_GPAD_UP) ? "joy-up" : "joy-down",
                      evt.data.gamepad.buttonNumber,
                      (int)evt.data.gamepad.flags,
                      evt.data.gamepad.dx,
                      evt.data.gamepad.dy);
                break;

            case HID_EVENT_GPAD_MOTION:
                printf("joy-motion\t(%d, %d)\n",
                      evt.data.gamepadMoved.dx,
                      evt.data.gamepadMoved.dy);
                break;

            default:
                printf("*** unknown\n");
        }
    }

    assert_int_ge(0, fd_cntl(fd, IOCMD_HID_FLUSH_EVENTS));
    assert_int_ge(0, fd_cntl(fd, IOCMD_HID_RELEASE_CURSOR));
    fd_close(fd);
}
