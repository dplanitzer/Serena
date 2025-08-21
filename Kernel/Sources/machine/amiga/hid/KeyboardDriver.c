//
//  KeyboardDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "KeyboardDriver.h"
#include <klib/RingBuffer.h>
#include <kpi/fcntl.h>
#include <machine/irq.h>
#include <machine/amiga/chipset.h>


// Keycode -> USB HID keyscan codes
// See: <http://whdload.de/docs/en/rawkey.html>
// See: <http://www.quadibloc.com/comp/scan.htm>
static const uint8_t g_usb_code_map[128] = {
    0x35, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2d, 0x2e, 0x31, 0x00, 0x62, // $00 - $0f
    0x14, 0x1a, 0x08, 0x15, 0x17, 0x1c, 0x18, 0x0c, 0x12, 0x13, 0x2f, 0x30, 0x00, 0x59, 0x5a, 0x5b, // $10 - $1f
    0x04, 0x16, 0x07, 0x09, 0x0a, 0x0b, 0x0d, 0x0e, 0x0f, 0x33, 0x34, 0x00, 0x00, 0x5c, 0x5d, 0x5e, // $20 - $2f
    0x36, 0x1d, 0x1b, 0x06, 0x19, 0x05, 0x11, 0x10, 0x36, 0x37, 0x38, 0x00, 0x63, 0x5f, 0x60, 0x61, // $30 - $3f
    0x2c, 0x2a, 0x2b, 0x58, 0x28, 0x29, 0x4c, 0x00, 0x00, 0x00, 0x56, 0x00, 0x52, 0x51, 0x4f, 0x50, // $40 - $4f
    0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x53, 0x54, 0x55, 0x56, 0x57, 0x75, // $50 - $5f
    0xe1, 0xe5, 0x39, 0xe0, 0xe2, 0xe6, 0xe3, 0xe7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // $60 - $6f
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, // $70 - $7f
};


final_class_ivars(KeyboardDriver, InputDriver,
    RingBuffer          keyQueue;   // irq state
    vcpu_t _Nullable    sigvp;      // irq state
    int                 signo;      // irq state
    int                 dropCount;  // irq state
);

IOCATS_DEF(g_cats, IOHID_KEYBOARD);


extern void KeyboardDriver_OnKeyboardInterrupt(KeyboardDriverRef _Nonnull self, int key);


errno_t KeyboardDriver_Create(CatalogId parentDirId, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    KeyboardDriverRef self;
    
    try(Driver_Create(class(KeyboardDriver), kDriver_Exclusive, parentDirId, g_cats, (DriverRef*)&self));
    try(RingBuffer_Init(&self->keyQueue, 16));

    *pOutSelf = (DriverRef)self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

static void KeyboardDriver_deinit(KeyboardDriverRef _Nonnull self)
{
    RingBuffer_Deinit(&self->keyQueue);
}

errno_t KeyboardDriver_onStart(DriverRef _Nonnull _Locked self)
{
    decl_try_err();

    DriverEntry de;
    de.dirId = Driver_GetParentDirectoryId(self);
    de.name = "kb";
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0444);
    de.driver = (HandlerRef)self;
    de.arg = 0;

    err = Driver_Publish(self, &de);
    if (err == EOK) {
        // Configure the keyboard serial port
        CIAA_BASE_DECL(ciaa);

        *CIA_REG_8(ciaa, CIA_CRA) = 0;

        irq_set_direct_handler(IRQ_ID_KEYBOARD, (irq_direct_func_t)KeyboardDriver_OnKeyboardInterrupt, self);
        irq_enable_src(IRQ_ID_CIA_A_SP);
    }
    return err;
}

void KeyboardDriver_onStop(DriverRef _Nonnull _Locked self)
{
    irq_disable_src(IRQ_ID_CIA_A_SP);
    Driver_Unpublish(self);
}

void KeyboardDriver_getReport(KeyboardDriverRef _Nonnull self, HIDReport* _Nonnull report)
{
    char keyCode;
    
    const unsigned sim = irq_set_mask(IRQ_MASK_KEYBOARD);
    const size_t r = RingBuffer_GetByte(&self->keyQueue, &keyCode);
    irq_set_mask(sim);

    if (r > 0) {
        report->type = (keyCode & 0x80) ? kHIDReportType_KeyUp : kHIDReportType_KeyDown;
        report->data.key.keyCode = (uint16_t)g_usb_code_map[keyCode & 0x7f];
    }
    else {
        report->type = kHIDReportType_Null;
    }
}

errno_t KeyboardDriver_setReportTarget(KeyboardDriverRef _Nonnull self, vcpu_t _Nullable vp, int signo)
{
    const unsigned sim = irq_set_mask(IRQ_MASK_KEYBOARD);
    self->sigvp = vp;
    self->signo = signo;
    irq_set_mask(sim);

    return EOK;
}

void KeyboardDriver_OnKeyboardInterrupt(KeyboardDriverRef _Nonnull self, int key)
{
    if (RingBuffer_PutByte(&self->keyQueue, (char)key) == 1) {
        if (self->sigvp) {
            vcpu_sigsend_irq(self->sigvp, self->signo, false);
        }
    }
    else {
        self->dropCount++;
    }
}


class_func_defs(KeyboardDriver, InputDriver,
override_func_def(deinit, KeyboardDriver, Object)
override_func_def(onStart, KeyboardDriver, Driver)
override_func_def(onStop, KeyboardDriver, Driver)
override_func_def(getReport, KeyboardDriver, InputDriver)
override_func_def(setReportTarget, KeyboardDriver, InputDriver)
);
