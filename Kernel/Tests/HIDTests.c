//
//  HIDTests.c
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
#include <sys/hidevent.h>
#include "Asserts.h"


void hid_test(int argc, char *argv[])
{
    const int fd = open("/dev/hid", O_RDONLY);
    HIDEvent evt;

    assertGreaterEqual(0, fd);
    printf("Waiting for events...\n");

    while (true) {
        const ssize_t nBytesRead = read(fd, &evt, sizeof(evt));
        
        assertGreaterEqual(0, nBytesRead);
        switch (evt.type) {
            case kHIDEventType_KeyDown:
            case kHIDEventType_KeyUp:
                printf("%s: $%hhx   flags: $%hhx  isRepeat: %s\n",
                      (evt.type == kHIDEventType_KeyUp) ? "KeyUp" : "KeyDown",
                      (int)evt.data.key.keyCode,
                      (int)evt.data.key.flags, evt.data.key.isRepeat ? "true" : "false");
                break;
                
            case kHIDEventType_FlagsChanged:
                printf("FlagsChanged: $%hhx\n", evt.data.flags.flags);
                break;
                
            case kHIDEventType_MouseUp:
            case kHIDEventType_MouseDown:
                printf("%s: %d   (%d, %d)   flags: $%hhx\n",
                      (evt.type == kHIDEventType_MouseUp) ? "MouseUp" : "MouseDown",
                      evt.data.mouse.buttonNumber,
                      evt.data.mouse.x,
                      evt.data.mouse.y,
                      (int)evt.data.mouse.flags);
                break;

            case kHIDEventType_MouseMoved:
                printf("MouseMoved   (%d, %d)\n",
                      evt.data.mouseMoved.x,
                      evt.data.mouseMoved.y);
                break;
                
            default:
                printf("*** unknown\n");
        }
    }
}
