//
//  KeyboardDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "KeyboardDriver.h"
#include <driver/hid/HIDManager.h>
#include <driver/hid/HIDKeyRepeater.h>
#include <hal/InterruptController.h>
#include <kpi/fcntl.h>


// Keycode -> USB HID keyscan codes
// See: <http://whdload.de/docs/en/rawkey.html>
// See: <http://www.quadibloc.com/comp/scan.htm>
static const uint8_t gUSBHIDKeycodes[128] = {
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
    const uint8_t* _Nonnull     keyCodeMap;
    HIDKeyRepeaterRef _Nonnull  keyRepeater;
    InterruptHandlerID          keyboardIrqHandler;
    InterruptHandlerID          vblIrqHandler;
);


extern void ksb_init(void);
extern int ksb_receive_key(void);
extern void ksb_acknowledge_key(void);
extern void KeyboardDriver_OnKeyboardInterrupt(KeyboardDriverRef _Nonnull self);
extern void KeyboardDriver_OnVblInterrupt(KeyboardDriverRef _Nonnull self);


errno_t KeyboardDriver_Create(DriverRef _Nullable parent, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    KeyboardDriverRef self;
    
    try(Driver_Create(class(KeyboardDriver), kDriver_Exclusive, parent, (DriverRef*)&self));
    
    self->keyCodeMap = gUSBHIDKeycodes;
    try(HIDKeyRepeater_Create(&self->keyRepeater));

    ksb_init();

    try(InterruptController_AddDirectInterruptHandler(gInterruptController,
                                                      INTERRUPT_ID_CIA_A_SP,
                                                      INTERRUPT_HANDLER_PRIORITY_NORMAL,
                                                      (InterruptHandler_Closure)KeyboardDriver_OnKeyboardInterrupt,
                                                      self,
                                                      &self->keyboardIrqHandler));
    InterruptController_SetInterruptHandlerEnabled(gInterruptController, self->keyboardIrqHandler, true);

    try(InterruptController_AddDirectInterruptHandler(gInterruptController,
                                                      INTERRUPT_ID_VERTICAL_BLANK,
                                                      INTERRUPT_HANDLER_PRIORITY_NORMAL - 1,
                                                      (InterruptHandler_Closure)KeyboardDriver_OnVblInterrupt,
                                                      self,
                                                      &self->vblIrqHandler));
    InterruptController_SetInterruptHandlerEnabled(gInterruptController, self->vblIrqHandler, true);

    *pOutSelf = (DriverRef)self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

static void KeyboardDriver_deinit(KeyboardDriverRef _Nonnull self)
{
    try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, self->keyboardIrqHandler));
    try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, self->vblIrqHandler));

    HIDKeyRepeater_Destroy(self->keyRepeater);
    self->keyRepeater = NULL;
}

errno_t KeyboardDriver_onStart(KeyboardDriverRef _Nonnull _Locked self)
{
    DriverEntry de;
    de.name = "kb";
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0444);
    de.arg = 0;

    return Driver_Publish((DriverRef)self, &de);
}

InputType KeyboardDriver_getInputType(KeyboardDriverRef _Nonnull self)
{
    return kInputType_Keyboard;
}

static void KeyboardDriver_GetKeyRepeatDelays(KeyboardDriverRef _Nonnull self, struct timespec* _Nullable pInitialDelay, struct timespec* _Nullable pRepeatDelay)
{
    const int irs = cpu_disable_irqs();
    HIDKeyRepeater_GetKeyRepeatDelays(self->keyRepeater, pInitialDelay, pRepeatDelay);
    cpu_restore_irqs(irs);
}

static void KeyboardDriver_SetKeyRepeatDelays(KeyboardDriverRef _Nonnull self, struct timespec initialDelay, struct timespec repeatDelay)
{
    const int irs = cpu_disable_irqs();
    HIDKeyRepeater_SetKeyRepeatDelays(self->keyRepeater, initialDelay, repeatDelay);
    cpu_restore_irqs(irs);
}

errno_t KeyboardDriver_ioctl(KeyboardDriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, int cmd, va_list ap)
{
    switch (cmd) {
        case kKeyboardCommand_GetKeyRepeatDelays: {
            struct timespec* initial = va_arg(ap, struct timespec*);
            struct timespec* repeat = va_arg(ap, struct timespec*);

            KeyboardDriver_GetKeyRepeatDelays(self, initial, repeat);
            return EOK;
        }

        case kKeyboardCommand_SetKeyRepeatDelays: {
            const struct timespec initial = va_arg(ap, struct timespec);
            const struct timespec repeat = va_arg(ap, struct timespec);

            KeyboardDriver_SetKeyRepeatDelays(self, initial, repeat);
            return EOK;
        }

        default:
            return super_n(ioctl, Driver, KeyboardDriver, self, pChannel, cmd, ap);
    }
}


void KeyboardDriver_OnKeyboardInterrupt(KeyboardDriverRef _Nonnull self)
{
    const uint8_t keyCode = ksb_receive_key();
    const HIDKeyState state = (keyCode & 0x80) ? kHIDKeyState_Up : kHIDKeyState_Down;
    const uint16_t code = (uint16_t)self->keyCodeMap[keyCode & 0x7f];

    if (code > 0) {
        HIDManager_ReportKeyboardDeviceChange(gHIDManager, state, code);
        if (state == kHIDKeyState_Up) {
            HIDKeyRepeater_KeyUp(self->keyRepeater, code);
        } else {
            HIDKeyRepeater_KeyDown(self->keyRepeater, code);
        }
    }
    ksb_acknowledge_key();
}

void KeyboardDriver_OnVblInterrupt(KeyboardDriverRef _Nonnull self)
{
    // const int = cpu_disable_irqs();
    HIDKeyRepeater_Tick(self->keyRepeater);
    // cpu_restore_irqs(irs);
}


class_func_defs(KeyboardDriver, InputDriver,
override_func_def(deinit, KeyboardDriver, Object)
override_func_def(onStart, KeyboardDriver, Driver)
override_func_def(getInputType, KeyboardDriver, InputDriver)
override_func_def(ioctl, KeyboardDriver, Driver)
);
