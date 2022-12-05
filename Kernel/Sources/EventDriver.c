//
//  EventDriver.c
//  Apollo
//
//  Created by Dietmar Planitzer on 5/31/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "EventDriver.h"
#include "Console.h"
#include "ConditionVariable.h"
#include "Heap.h"
#include "InterruptController.h"
#include "InputDriver.h"
#include "Lock.h"
#include "MonotonicClock.h"
#include "RingBuffer.h"
#include "USBHIDKeys.h"
#include "VirtualProcessor.h"
#include "VirtualProcessorPool.h"
#include "VirtualProcessorScheduler.h"


// The event driver runs a deticated virtual processor which processes events
// from low-level input drivers. It creates HID events and makes them available
// via the EventDriver_GetEvents() call.
//
// The event VP runs a loop which is driven by the vertical blank interrupt. The
// driver sleeps until the next vertical blank interrupt occurs and it then
// gathers all queued up low-level events.
//
// Low-level input drivers provide low-level events in two different ways:
//
// 1) via a ring buffer: the keyboard driver posts key codes to a key queue which
//    is a ring buffer. The event driver collects those key codes at the next
//    vertical blank.
//
// 2) via querying for a report: the mouse, joystick, etc input drivers provide
//    the current device state to the event driver when it asks for it at the
//    next vertical blank.
//
// This model has the advantages that it makes the implementation simpler and that
// it is very lightweight because low-level events do not cause context switches.
// We only need to do a single context switch per vertical blank. Since event
// delivery is done once per vertical blank, this model minimizes overhead.


// Per port input controller state
typedef struct _InputControllerState {
    InputControllerType     type;
    union {
        struct _MouseState {
            MouseDriverRef _Nonnull driver;
        }   mouse;
        
        struct _DigitalJoystickState {
            DigitalJoystickDriverRef _Nonnull   driver;
            JoystickReport                      previous;       // hardware state corresponding to most recently posted events
        }   digitalJoystick;
        
        struct _AnalogJoystickState {
            AnalogJoystickDriverRef _Nonnull    driver;
            JoystickReport                      previous;       // hardware state corresponding to most recently posted events
        }   analogJoystick;
        
        struct _LightPenState {
            LightPenDriverRef _Nonnull  driver;
            // XXX is state per light pen or map to mouse state?
        }   lightPen;
    }                       u;
} InputControllerState;


// State to handle auto-repeat for a single key. We support up to 5 simultaneously
// auto-repeating keys
#define KEY_REPEATERS_COUNT 5

enum {
    kKeyRepeaterState_Available = 0,
    kKeyRepeaterState_InitialDelay,
    kKeyRepeaterState_Repeating
};


typedef struct _KeyRepeater {
    // XXX Should be TimeInterval. However vbcc generates incorrect code if we use TimeInterval here (accesses the wrong struct fields)
    Int32           start_time_seconds;
    Int32           start_time_nanoseconds;
    HIDKeyCode      usbcode;
    UInt16          state;
    UInt32          func_flags;
} KeyRepeater;


#define EVENT_QUEUE_MAX_EVENTS      32
#define KEY_MAP_INTS_COUNT          (256/32)
#define MAX_INPUT_CONTROLLER_PORTS  2
typedef struct _EventDriver {
    KeyboardDriverRef _Nonnull  keyboard_driver;
    InputControllerState        port[MAX_INPUT_CONTROLLER_PORTS];
    
    VirtualProcessor* _Nonnull  event_vp;
    Lock                        lock;
    ConditionVariable           event_queue_cv;
    RingBuffer                  event_queue;
    
    BinarySemaphore             vb_sema;    // Vertical blank semaphore
    InterruptHandlerID          vb_irq;

    UInt32                      key_map[KEY_MAP_INTS_COUNT];    // keycode is the bit index. 1 -> key down; 0 -> key up
    UInt32                      key_modifierFlags;
    KeyRepeater                 key_repeater[KEY_REPEATERS_COUNT];
    TimeInterval                key_initial_repeat_delay;       // [200ms...3s]
    TimeInterval                key_repeat_delay;               // [20ms...2s]
    HIDKeyCode                  last_keycode;
    
    GraphicsDriverRef _Nonnull  gdevice;
    Int16                       screen_rect_x;
    Int16                       screen_rect_y;
    Int16                       screen_rect_w;
    Int16                       screen_rect_h;
    Int16                       mouse_x;
    Int16                       mouse_y;
    UInt32                      mouse_buttons;              // most recent state of the mouse buttons as reported by the mouse driver
    UInt32                      mouse_prev_buttons_state;   // state of the mouse buttons corresponding to the most recently posted  button events
    volatile Int                mouseCursorHiddenCounter;
    volatile Bool               isMouseCursorObscured;      // hidden until the mouse moves
    volatile Bool               mouseConfigDidChange;       // tells the event VP to apply the new mouse config to the graphics driver
} EventDriver;


static void EventDriver_CreateInputControllerForPort(EventDriverRef _Nonnull pDriver, InputControllerType type, Int portId);
static void EventDriver_DestroyInputControllerForPort(EventDriverRef _Nonnull pDriver, Int portId);
static void EventDriver_Run(EventDriver* _Nonnull pDriver);


EventDriverRef _Nullable EventDriver_Create(GraphicsDriverRef _Nonnull gdevice)
{
    EventDriver* pDriver = (EventDriver*)kalloc(sizeof(EventDriver), HEAP_ALLOC_OPTION_CLEAR);
    FailNULL(pDriver);
    
    VirtualProcessorAttributes attribs;
    VirtualProcessorAttributes_Init(&attribs);
    attribs.priority = VP_PRIORITY_REALTIME + 4;
    attribs.userStackSize = 0;
    pDriver->event_vp = VirtualProcessorPool_AcquireVirtualProcessor(VirtualProcessorPool_GetShared(), &attribs, (VirtualProcessor_Closure)EventDriver_Run, (Byte*)pDriver, false);
    FailNULL(pDriver->event_vp);

    FailFalse((RingBuffer_Init(&pDriver->event_queue, EVENT_QUEUE_MAX_EVENTS * sizeof(HIDEvent))));
    
    ConditionVariable_Init(&pDriver->event_queue_cv);
    BinarySemaphore_Init(&pDriver->vb_sema, true);
    Lock_Init(&pDriver->lock);
    
    pDriver->key_modifierFlags = 0;
    pDriver->last_keycode = KEY_NONE;
    pDriver->gdevice = gdevice;
    pDriver->screen_rect_x = 0;
    pDriver->screen_rect_y = 0;
    pDriver->screen_rect_w = (Int16)GraphicsDriver_GetFramebufferSize(gdevice).width;
    pDriver->screen_rect_h = (Int16)GraphicsDriver_GetFramebufferSize(gdevice).height;
    pDriver->mouse_x = 0;
    pDriver->mouse_y = 0;
    pDriver->mouse_buttons = 0;
    pDriver->mouse_prev_buttons_state = 0;
    pDriver->mouseCursorHiddenCounter = 0;
    pDriver->isMouseCursorObscured = false;
    pDriver->mouseConfigDidChange = true;

    for (Int i = 0; i < KEY_MAP_INTS_COUNT; i++) {
        pDriver->key_map[i] = 0;
    }
    pDriver->key_initial_repeat_delay = TimeInterval_MakeMilliseconds(300);
    pDriver->key_repeat_delay = TimeInterval_MakeMilliseconds(10);
    for (Int i = 0; i < KEY_REPEATERS_COUNT; i++) {
        pDriver->key_repeater[i].state = kKeyRepeaterState_Available;
    }
    
    pDriver->vb_irq = InterruptController_AddBinarySemaphoreInterruptHandler(InterruptController_GetShared(),
                                                                             INTERRUPT_ID_VERTICAL_BLANK,
                                                                             INTERRUPT_HANDLER_PRIORITY_HIGHEST - 1,
                                                                             &pDriver->vb_sema);
    FailZero(pDriver->vb_irq);
    
    
    // Open the input devices
    pDriver->keyboard_driver = KeyboardDriver_Create(pDriver);
    assert(pDriver->keyboard_driver != NULL);

    for (Int i = 0; i < MAX_INPUT_CONTROLLER_PORTS; i++) {
        pDriver->port[i].type = kInputControllerType_None;
    }
    
    EventDriver_CreateInputControllerForPort(pDriver, kInputControllerType_Mouse, 0);

    InterruptController_SetInterruptHandlerEnabled(InterruptController_GetShared(), pDriver->vb_irq, true);
    VirtualProcessor_Resume(pDriver->event_vp, false);

    return pDriver;
    
failed:
    EventDriver_Destroy(pDriver);
    return NULL;
}

void EventDriver_Destroy(EventDriverRef _Nullable pDriver)
{
    if (pDriver) {
        KeyboardDriver_Destroy(pDriver->keyboard_driver);
        for (Int i = 0; i < MAX_INPUT_CONTROLLER_PORTS; i++) {
            EventDriver_DestroyInputControllerForPort(pDriver, i);
        }
        
        pDriver->gdevice = NULL;
        InterruptController_RemoveInterruptHandler(InterruptController_GetShared(), pDriver->vb_irq);
        Lock_Deinit(&pDriver->lock);
        BinarySemaphore_Deinit(&pDriver->vb_sema);
        ConditionVariable_Deinit(&pDriver->event_queue_cv);
        RingBuffer_Deinit(&pDriver->event_queue);
        
        kfree((Byte*)pDriver);
    }
}

// Creates a new input controller driver instance for the port 'portId'. Expects that the port
// is currently unassigned (aka type is == 'none').
static void EventDriver_CreateInputControllerForPort(EventDriverRef _Nonnull pDriver, InputControllerType type, Int portId)
{
    switch (type) {
        case kInputControllerType_None:
            break;
            
        case kInputControllerType_Mouse:
            pDriver->port[portId].u.mouse.driver = MouseDriver_Create(pDriver, portId);
            assert(pDriver->port[portId].u.mouse.driver != NULL);
            break;
            
        case kInputControllerType_DigitalJoystick:
            pDriver->port[portId].u.digitalJoystick.driver = DigitalJoystickDriver_Create(pDriver, portId);
            assert(pDriver->port[portId].u.digitalJoystick.driver != NULL);
            pDriver->port[portId].u.digitalJoystick.previous.buttonsDown = 0;
            pDriver->port[portId].u.digitalJoystick.previous.xAbs = 0;
            pDriver->port[portId].u.digitalJoystick.previous.yAbs = 0;
            break;
            
        case kInputControllerType_AnalogJoystick:
            pDriver->port[portId].u.analogJoystick.driver = AnalogJoystickDriver_Create(pDriver, portId);
            assert(pDriver->port[portId].u.analogJoystick.driver != NULL);
            pDriver->port[portId].u.analogJoystick.previous.buttonsDown = 0;
            pDriver->port[portId].u.analogJoystick.previous.xAbs = 0;
            pDriver->port[portId].u.analogJoystick.previous.yAbs = 0;
            break;
            
        case kInputControllerType_LightPen:
            pDriver->port[portId].u.lightPen.driver = LightPenDriver_Create(pDriver, portId);
            assert(pDriver->port[portId].u.lightPen.driver != NULL);
            break;
            
        default:
            abort();
    }
    
    pDriver->port[portId].type = type;
}

// Destroys the input controller that is configured for port 'portId'. This frees the input controller
// specific driver and all associated state
static void EventDriver_DestroyInputControllerForPort(EventDriverRef _Nonnull pDriver, Int portId)
{
    switch (pDriver->port[portId].type) {
        case kInputControllerType_None:
            break;
            
        case kInputControllerType_Mouse:
            MouseDriver_Destroy(pDriver->port[portId].u.mouse.driver);
            pDriver->port[portId].u.mouse.driver = NULL;
            break;
            
        case kInputControllerType_DigitalJoystick:
            DigitalJoystickDriver_Destroy(pDriver->port[portId].u.digitalJoystick.driver);
            pDriver->port[portId].u.digitalJoystick.driver = NULL;
            break;
            
        case kInputControllerType_AnalogJoystick:
            AnalogJoystickDriver_Destroy(pDriver->port[portId].u.analogJoystick.driver);
            pDriver->port[portId].u.analogJoystick.driver = NULL;
            break;
            
        case kInputControllerType_LightPen:
            LightPenDriver_Destroy(pDriver->port[portId].u.lightPen.driver);
            pDriver->port[portId].u.lightPen.driver = NULL;
            break;
            
        default:
            abort();
    }
    
    pDriver->port[portId].type = kInputControllerType_None;
}

GraphicsDriverRef _Nonnull EventDriver_GetGraphicsDriver(EventDriverRef _Nonnull pDriver)
{
    return pDriver->gdevice;
}

InputControllerType EventDriver_GetInputControllerTypeForPort(EventDriverRef _Nonnull pDriver, Int portId)
{
    InputControllerType type;
    
    assert(portId >= 0 && portId < MAX_INPUT_CONTROLLER_PORTS);
    
    Lock_Lock(&pDriver->lock);
    type = pDriver->port[portId].type;
    Lock_Unlock(&pDriver->lock);
    
    return type;
}

void EventDriver_SetInputControllerTypeForPort(EventDriverRef _Nonnull pDriver, InputControllerType type, Int portId)
{
    assert(portId >= 0 && portId < MAX_INPUT_CONTROLLER_PORTS);

    Lock_Lock(&pDriver->lock);
    EventDriver_DestroyInputControllerForPort(pDriver, portId);
    EventDriver_CreateInputControllerForPort(pDriver, type, portId);
    Lock_Unlock(&pDriver->lock);
}

void EventDriver_GetKeyRepeatDelays(EventDriverRef _Nonnull pDriver, TimeInterval* _Nullable pInitialDelay, TimeInterval* _Nullable pRepeatDelay)
{
    if (pInitialDelay) {
        *pInitialDelay = pDriver->key_initial_repeat_delay;
    }
    if (pRepeatDelay) {
        *pRepeatDelay = pDriver->key_repeat_delay;
    }
}

void EventDriver_SetKeyRepeatDelays(EventDriverRef _Nonnull pDriver, TimeInterval initialDelay, TimeInterval repeatDelay)
{
    pDriver->key_initial_repeat_delay = initialDelay;
    pDriver->key_repeat_delay = repeatDelay;
}

// Show the mouse cursor. This decrements the hidden counter. The mouse cursor
// is only shown if this counter reaches zero. The operation is carried out at
// the next vertical blank.
void EventDriver_ShowMouseCursor(EventDriverRef _Nonnull pDriver)
{
    Lock_Lock(&pDriver->lock);
    pDriver->mouseCursorHiddenCounter--;
    if (pDriver->mouseCursorHiddenCounter < 0) {
        pDriver->mouseCursorHiddenCounter = 0;
    }
    pDriver->mouseConfigDidChange = true;
    Lock_Unlock(&pDriver->lock);
}

// Hides the mouse cursor. This increments the hidden counter. The mouse remains
// hidden as long as the counter does not reach the value zero. The operation is
// carried out at the next vertical blank.
void EventDriver_HideMouseCursor(EventDriverRef _Nonnull pDriver)
{
    Lock_Lock(&pDriver->lock);
    pDriver->mouseCursorHiddenCounter++;
    pDriver->mouseConfigDidChange = true;
    Lock_Unlock(&pDriver->lock);
}

// Obscures the mouse cursor. This hides the mouse cursor until the user moves
// the mouse or clicks a button. The change is carried out at the next vertical
// blank.
void EventDriver_ObscureMouseCursor(EventDriverRef _Nonnull pDriver)
{
    Lock_Lock(&pDriver->lock);
    pDriver->isMouseCursorObscured = true;
    pDriver->mouseConfigDidChange = true;
    Lock_Unlock(&pDriver->lock);
}

// Returns the current mouse location in screen space.
Point EventDriver_GetMouseLocation(EventDriverRef _Nonnull pDriver)
{
    Point loc;
    
    Lock_Lock(&pDriver->lock);
    loc.x = pDriver->mouse_x;
    loc.y = pDriver->mouse_y;
    Lock_Unlock(&pDriver->lock);
    
    return loc;
}

// Returns a bit mask of all the mouse buttons that are currently pressed.
UInt32 EventDriver_GetMouseButtonsDown(EventDriverRef _Nonnull pDriver)
{
    UInt32 buttons;
    
    Lock_Lock(&pDriver->lock);
    buttons = pDriver->mouse_buttons;
    Lock_Unlock(&pDriver->lock);
    
    return buttons;
}

inline void KeyMap_SetKeyDown(UInt32* _Nonnull pKeyMap, HIDKeyCode keycode)
{
    assert(keycode <= 255);
    const UInt32 wordIdx = keycode >> 5;
    const UInt32 bitIdx = keycode - (wordIdx << 5);
    
    pKeyMap[wordIdx] |= (1 << bitIdx);
}

inline Bool KeyMap_IsKeyDown(const UInt32* _Nonnull pKeyMap, HIDKeyCode keycode)
{
    assert(keycode <= 255);
    const UInt32 wordIdx = keycode >> 5;
    const UInt32 bitIdx = keycode - (wordIdx << 5);
    
    return (pKeyMap[wordIdx] & (1 << bitIdx)) != 0 ? true : false;
}

// Returns the keycodes of the keys that are currently pressed. All pressed keys
// are returned if 'nKeysToCheck' is 0. Only the keys which are pressed and in the
// set 'pKeysToCheck' are returned if 'nKeysToCheck' is > 0.
void EventDriver_GetKeysDown(EventDriverRef _Nonnull pDriver, const HIDKeyCode* _Nullable pKeysToCheck, Int nKeysToCheck, HIDKeyCode* _Nullable pKeysDown, Int* _Nonnull nKeysDown)
{
    Int oi = 0;
    
    Lock_Lock(&pDriver->lock);
    if (nKeysToCheck > 0 && pKeysToCheck) {
        if (pKeysDown) {
            // Returns at most 'nKeysDown' keys which are in the set 'pKeysToCheck'
            for (Int i = 0; i < min(nKeysToCheck, *nKeysDown); i++) {
                if (KeyMap_IsKeyDown(pDriver->key_map, pKeysToCheck[i])) {
                    pKeysDown[oi++] = pKeysToCheck[i];
                }
            }
        } else {
            // Return the number of keys that are down and which are in the set 'pKeysToCheck'
            for (Int i = 0; i < nKeysToCheck; i++) {
                if (KeyMap_IsKeyDown(pDriver->key_map, pKeysToCheck[i])) {
                    oi++;
                }
            }
        }
    } else {
        if (pKeysDown) {
            // Return all keys that are down
            for (Int i = 0; i < *nKeysDown; i++) {
                if (KeyMap_IsKeyDown(pDriver->key_map, (HIDKeyCode)i)) {
                    pKeysDown[oi++] = (HIDKeyCode)i;
                }
            }
        } else {
            // Return the number of keys that are down
            for (Int i = 0; i < nKeysToCheck; i++) {
                if (KeyMap_IsKeyDown(pDriver->key_map, (HIDKeyCode)i)) {
                    oi++;
                }
            }
        }
    }
    Lock_Unlock(&pDriver->lock);
    
    *nKeysDown = oi;
}

// Posts a copy of the given event to the event queue. The oldest queued event
// is removed and effectively replaced by the new event if the event queue is
// full.
// 1) Expects that the caller has already locked the event queue.
// 2) Expects that the event contains the appropriate eventTime
//
// For internal use by the event generation code only!
void EventDriver_PutEvent_Locked(EventDriverRef _Nonnull pDriver, HIDEvent* _Nonnull pEvent)
{
    if (RingBuffer_PutBytes(&pDriver->event_queue, (const Byte*)pEvent, sizeof(HIDEvent)) == 0) {
        // Queue is full. Remove the oldest event and copy in the new one
        HIDEvent dummy;
        
        RingBuffer_GetBytes(&pDriver->event_queue, (Byte*)&dummy, sizeof(HIDEvent));
        RingBuffer_PutBytes(&pDriver->event_queue, (const Byte*)pEvent, sizeof(HIDEvent));
    }
}

// Posts a copy of the given event to the event queue. The event time is set to
// the current time. The oldest queued event is removed and effectively replaced
// by the new event if the event queue is full.
void EventDriver_PostEvent(EventDriverRef _Nonnull pDriver, const HIDEvent* _Nonnull pEvent)
{
    HIDEvent newEvent = *pEvent;
    newEvent.eventTime = MonotonicClock_GetCurrentTime();
    
    Lock_Lock(&pDriver->lock);
    EventDriver_PutEvent_Locked(pDriver, &newEvent);
    ConditionVariable_Broadcast(&pDriver->event_queue_cv, &pDriver->lock);
}

// Blocks the caller until either events have arrived or 'deadline' has passed
// (whatever happens earlier). Then copies at most 'pEventCount' events to the
// provided buffer 'pEvents'. On return 'pEventCount' is set to the actual
// number of events copied out. An error is returned if the wait was interrupted
// or a timeout has occured.
ErrorCode EventDriver_GetEvents(EventDriverRef _Nonnull pDriver, HIDEvent* _Nonnull pEvents, Int* _Nonnull pEventCount, TimeInterval deadline)
{
    ErrorCode err = EOK;
    
    if (*pEventCount > 0) {
        Lock_Lock(&pDriver->lock);
        
        while (RingBuffer_IsEmpty(&pDriver->event_queue) && err == EOK) {
            err = ConditionVariable_Wait(&pDriver->event_queue_cv, &pDriver->lock, deadline);
        }
        
        if (err == EOK) {
            const Int nEventsToCopy = min(RingBuffer_ReadableCount(&pDriver->event_queue), *pEventCount);
            const Int nCopiedBytes = RingBuffer_GetBytes(&pDriver->event_queue, (Byte*)pEvents, nEventsToCopy*sizeof(HIDEvent));
            
            *pEventCount = nEventsToCopy;
        }
        
        Lock_Unlock(&pDriver->lock);
    }
    
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Key auto repeats
////////////////////////////////////////////////////////////////////////////////


// Returns true if the given key should be auto-repeated
static Bool EventDriver_ShouldAutoRepeatKeycode(EventDriver* _Nonnull pDriver, HIDKeyCode usbcode)
{
    // Everything except:
    // - modifier keys
    // - caps lock
    // - some function keys (print screen, etc)
    // - key controller messages (errors)
    switch (usbcode) {
        case KEY_LEFTCTRL:
        case KEY_LEFTSHIFT:
        case KEY_LEFTALT:
        case KEY_LEFTMETA:
        case KEY_RIGHTCTRL:
        case KEY_RIGHTSHIFT:
        case KEY_RIGHTALT:
        case KEY_RIGHTMETA:

        case KEY_CAPSLOCK:
        case KEY_SYSRQ:
        case KEY_SCROLLLOCK:
        case KEY_NUMLOCK:
        case KEY_POWER:
            
        case KEY_NONE:
        case KEY_ERR_OVF:
        case KEY_ERR_POST:
        case KEY_ERR_UNDEF:
            return false;
            
        default:
            return true;
    }
}

// Returns the auto-repeater for the given key code. Null is returned if there is no
// auto-repeater for this keycode.
inline KeyRepeater* _Nullable EventDriver_GetKeyRepeaterForKeycode(EventDriver* _Nonnull pDriver, HIDKeyCode usbcode)
{
    for (Int i = 0; i < KEY_REPEATERS_COUNT; i++) {
        if (pDriver->key_repeater[i].state != kKeyRepeaterState_Available && pDriver->key_repeater[i].usbcode == usbcode) {
            return &pDriver->key_repeater[i];
        }
    }
    
    return NULL;
}

// Returns the first available auto-repeater. Null is returned if no more auto-repeater
// is available.
inline KeyRepeater* _Nullable EventDriver_GetAvailableKeyRepeater(EventDriver* _Nonnull pDriver)
{
    for (Int i = 0; i < KEY_REPEATERS_COUNT; i++) {
        if (pDriver->key_repeater[i].state == kKeyRepeaterState_Available) {
            return &pDriver->key_repeater[i];
        }
    }
    
    return NULL;
}

// Initializes a key auto-repeater and moves it to the initial auto-repeat delay state.
static void KeyRepeater_StartInitDelay(KeyRepeater* _Nonnull pRepeater, HIDKeyCode usbcode, UInt32 funcFlags, TimeInterval curTime)
{
    pRepeater->state = kKeyRepeaterState_InitialDelay;
    pRepeater->usbcode = usbcode;
    pRepeater->func_flags = funcFlags;
    pRepeater->start_time_seconds = curTime.seconds;
    pRepeater->start_time_nanoseconds = curTime.nanoseconds;
}

// Cancels a key auto-repeater and makes it available for reuse.
static void KeyRepeater_Cancel(KeyRepeater* _Nonnull pRepeater)
{
    pRepeater->state = kKeyRepeaterState_Available;
    pRepeater->usbcode = 0;
    pRepeater->func_flags = 0;
    pRepeater->start_time_seconds = 0;
    pRepeater->start_time_nanoseconds = 0;
}

// Update the key auto-repeater state and return true if an auto-repeat event should
// be generated.
static Bool KeyRepeater_ShouldRepeat(KeyRepeater* _Nonnull pRepeater, EventDriver* _Nonnull pDriver, TimeInterval curTime)
{
    switch (pRepeater->state) {
        case kKeyRepeaterState_InitialDelay: {
            const TimeInterval diff = TimeInterval_Subtract(curTime, TimeInterval_Make(pRepeater->start_time_seconds, pRepeater->start_time_nanoseconds));
            if (TimeInterval_GreaterEquals(diff, pDriver->key_initial_repeat_delay)) {
                pRepeater->state = kKeyRepeaterState_Repeating;
                pRepeater->start_time_seconds = curTime.seconds;
                pRepeater->start_time_nanoseconds = curTime.nanoseconds;
                return true;
            }
            break;
        }

        case kKeyRepeaterState_Repeating: {
            const TimeInterval diff = TimeInterval_Subtract(curTime, TimeInterval_Make(pRepeater->start_time_seconds, pRepeater->start_time_nanoseconds));
            if (TimeInterval_GreaterEquals(diff, pDriver->key_repeat_delay)) {
                pRepeater->start_time_seconds = curTime.seconds;
                pRepeater->start_time_nanoseconds = curTime.nanoseconds;
                return true;
            }
            break;
        }

        case kKeyRepeaterState_Available:
            break;
    }

    return false;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Event generation and posting
////////////////////////////////////////////////////////////////////////////////


// USB Keycode -> kHIDEventModifierFlag_XXX values which are be or'd / and'd into pDriver->key_modifierFlags.
// Bit 7 indicates whether the key is left or right: 0 -> left; 1 -> right
// ctrl     0x04
// shift    0x02
// option   0x08
// command  0x10
// capslock 0x01
// numpad   0x20
// func     0x40
static const UInt8 gUSBHIDKeyFlags[256] = {
    0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // $00 - $0f
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // $10 - $1f
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, // $20 - $2f
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, // $30 - $3f
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, // $40 - $4f
    0x40, 0x40, 0x40, 0x60, 0x20, 0x20, 0x20, 0x20, 0x60, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // $50 - $5f
    0x20, 0x20, 0x20, 0x20, 0x00, 0x40, 0x40, 0x20, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, // $60 - $6f
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, // $70 - $7f
    
    0x40, 0x40, 0x40, 0x40, 0x40, 0x20, 0x20, 0x40, 0x40, 0x40, 0x40, 0x40, 0x20, 0x40, 0x40, 0x40, // $80 - $8f
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, // $90 - $9f
    0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // $a0 - $af
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x60, 0x60, 0x20, 0x20, 0x20, 0x20, // $b0 - $bf
    0x20, 0x20, 0x60, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // $c0 - $cf
    0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x00, 0x00, // $d0 - $df
    0x04, 0x02, 0x08, 0x10, 0x84, 0x82, 0x88, 0x90, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, // $e0 - $ef
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, // $f0 - $ff
};

// Updates the modifier flags state. Returns true if the key has changed the modifier
// flags; false otherwise
static Bool EventDriver_UpdateFlags(EventDriverRef _Nonnull pDriver, const KeyboardReport* _Nonnull pReport)
{
    const HIDKeyCode key = pReport->keycode;
    const UInt32 modFlags = (key <= 255) ?gUSBHIDKeyFlags[key] & 0x1f : 0;
    
    if (modFlags != 0) {
        const Bool isRight = (key <= 255) ? gUSBHIDKeyFlags[key] & 0x80 : 0;
        const UInt32 devModFlags = (isRight) ? modFlags << 16 : modFlags << 24;

        if (pReport->isKeyUp) {
            pDriver->key_modifierFlags &= ~modFlags;
            pDriver->key_modifierFlags &= ~devModFlags;
        } else {
            pDriver->key_modifierFlags |= modFlags;
            pDriver->key_modifierFlags |= devModFlags;
        }
        return true;
    }
    return false;
}

static void EventDriver_GenerateKeyboardEvents(EventDriverRef _Nonnull pDriver, TimeInterval curTime)
{
    KeyboardReport report;
    HIDEvent evt;
    
    // Process queued up low-level key code events
    while (true) {
        if (!KeyboardDriver_GetReport(pDriver->keyboard_driver, &report)) {
            break;
        }
        
        const Bool isModifierKey = EventDriver_UpdateFlags(pDriver, &report);
        
        if (report.keycode > 0) {
            const UInt32 keyFunc = (report.keycode <= 255) ? gUSBHIDKeyFlags[report.keycode] & 0x60 : 0;
            const UInt32 flags = pDriver->key_modifierFlags | keyFunc;
            const UInt32 funcFlags = keyFunc;
            const Bool isCapsLockPair = (report.keycode == KEY_CAPSLOCK && pDriver->last_keycode == KEY_CAPSLOCK);
            
            // We may receive a caps-lock key down / up twice in a row. Coalasce it into one
            if (isCapsLockPair) {
                pDriver->last_keycode = KEY_NONE;
                continue;
            }
            
            
            // Cancel all auto-repeaters if the key queue overflowed because we might
            // have lost a significant key up / key down
            if (report.keycode == KEY_ERR_OVF || report.keycode == KEY_ERR_UNDEF) {
                pDriver->key_modifierFlags = 0;
                pDriver->last_keycode = KEY_NONE;
                for (Int j = 0; j < KEY_REPEATERS_COUNT; j++) {
                    KeyRepeater_Cancel(&pDriver->key_repeater[j]);
                }
            }
            
            
            // Key auto-repeater setup:
            // - start an auto-repeater if we have one available and the key is a key down and eligible for auto-repeat
            // - cancel an auto-repeater if one exists for the key code and this is a key up or key down
            //   (key down means we lost the key up; this allows the user to get the key unstuck by hitting it again)
            KeyRepeater* pRepeater = EventDriver_GetKeyRepeaterForKeycode(pDriver, report.keycode);
            if (pRepeater) {
                KeyRepeater_Cancel(pRepeater);
            } else if (!report.isKeyUp && EventDriver_ShouldAutoRepeatKeycode(pDriver, report.keycode)) {
                pRepeater = EventDriver_GetAvailableKeyRepeater(pDriver);
                if (pRepeater) {
                    KeyRepeater_StartInitDelay(pRepeater, report.keycode, funcFlags, curTime);
                }
            }
            
            
            // Update the key map
            KeyMap_SetKeyDown(pDriver->key_map, report.keycode);
            
            
            // Post the key event
            if (!isModifierKey) {
                evt.type = (report.isKeyUp) ? kHIDEventType_KeyUp : kHIDEventType_KeyDown;
            } else {
                evt.type = kHIDEventType_FlagsChanged;
            }
            evt.eventTime = curTime;
            evt.data.key.keycode = report.keycode;
            evt.data.key.isRepeat = false;
            evt.data.key.flags = flags;
            EventDriver_PutEvent_Locked(pDriver, &evt);
            
            pDriver->last_keycode = report.keycode;
        }
    }
    
    
    // Generate auto-repeated key events if necessary
    for (Int i = 0; i < KEY_REPEATERS_COUNT; i++) {
        KeyRepeater* pRepeater = &pDriver->key_repeater[i];
        
        if (KeyRepeater_ShouldRepeat(pRepeater, pDriver, curTime)) {
            evt.type = kHIDEventType_KeyDown;
            evt.eventTime = curTime;
            evt.data.key.keycode = pRepeater->usbcode;
            evt.data.key.isRepeat = true;
            evt.data.key.flags = pDriver->key_modifierFlags | pRepeater->func_flags;
            EventDriver_PutEvent_Locked(pDriver, &evt);
        }
    }
}

static void EventDriver_GenerateMouseEvents(EventDriverRef _Nonnull pDriver, TimeInterval curTime)
{
    HIDEvent evt;
    const UInt32 old_buttons = pDriver->mouse_prev_buttons_state;
    const UInt32 new_buttons = pDriver->mouse_buttons;

    // Generate mouse button up/down events
    if (new_buttons != old_buttons) {
        // XXX should be able to ask the mouse input driver how many buttons it supports
        for (Int i = 0; i < 3; i++) {
            const UInt32 old_down = old_buttons & (1 << i);
            const UInt32 new_down = new_buttons & (1 << i);
            
            if (old_down ^ new_down) {
                if (old_down == 0 && new_down != 0) {
                    evt.type = kHIDEventType_MouseDown;
                } else {
                    evt.type = kHIDEventType_MouseUp;
                }
                evt.eventTime = curTime;
                evt.data.mouse.buttonNumber = i;
                evt.data.mouse.flags = pDriver->key_modifierFlags;
                evt.data.mouse.location.x = pDriver->mouse_x;
                evt.data.mouse.location.y = pDriver->mouse_y;
                EventDriver_PutEvent_Locked(pDriver, &evt);
            }
        }
    }
    
    pDriver->mouse_prev_buttons_state = pDriver->mouse_buttons;
}

static void EventDriver_UpdateMouseState(EventDriverRef _Nonnull pDriver)
{
    const Int16 old_mouse_x = pDriver->mouse_x;
    const Int16 old_mouse_y = pDriver->mouse_y;
    const Point mouse_cursor_hotspot = GraphicsDriver_GetMouseCursorHotSpot(pDriver->gdevice);
    
    // Compute the mouse rect. The area inside of which the mouse may freely move.
    const Int16 mouse_min_x = pDriver->screen_rect_x;
    const Int16 mouse_min_y = pDriver->screen_rect_y;
    const Int16 mouse_max_x = mouse_min_x + max((pDriver->screen_rect_w - 1) - mouse_cursor_hotspot.x, 0);
    const Int16 mouse_max_y = mouse_min_y + max((pDriver->screen_rect_h - 1) - mouse_cursor_hotspot.y, 0);
    
    
    // Update the mouse position and buttons
    Bool hasMouseStateChanged = false;
    
    pDriver->mouse_buttons = 0;
    
    for (Int i = 0; i < MAX_INPUT_CONTROLLER_PORTS; i++) {
        if (pDriver->port[i].type == kInputControllerType_Mouse) {
            MouseReport report;
            
            MouseDriver_GetReport(pDriver->port[i].u.mouse.driver, &report);
            pDriver->mouse_x += report.xDelta;
            pDriver->mouse_y += report.yDelta;
            pDriver->mouse_x = min(max(pDriver->mouse_x, mouse_min_x), mouse_max_x);
            pDriver->mouse_y = min(max(pDriver->mouse_y, mouse_min_y), mouse_max_y);
            pDriver->mouse_buttons |= report.buttonsDown;
            
            if (report.xDelta != 0 || report.yDelta != 0 || report.buttonsDown != 0) {
                hasMouseStateChanged = true;
            }
        }
    }
    
    
    if (pDriver->mouse_x != old_mouse_x || pDriver->mouse_y != old_mouse_y) {
        GraphicsDriver_SetMouseCursorPosition(pDriver->gdevice, pDriver->mouse_x, pDriver->mouse_y);
    }
    
    if (pDriver->mouseConfigDidChange) {
        Bool doShow = false;
        
        pDriver->mouseConfigDidChange = false;
        
        // Unobscure the mouse cursor if it is obscured, supposed to be visible and the mouse has moved or the user has clicked
        if (pDriver->isMouseCursorObscured && pDriver->mouseCursorHiddenCounter == 0 && hasMouseStateChanged) {
            pDriver->isMouseCursorObscured = false;
            doShow = true;
        }
        
        
        // Show / hide the mouse cursor if needed
        if (pDriver->mouseCursorHiddenCounter > 0) {
            doShow = false;
        } else if (!pDriver->isMouseCursorObscured) {
            doShow = true;
        }
        
        GraphicsDriver_SetMouseCursorVisible(pDriver->gdevice, doShow);
    }
}

static void EventDriver_GenerateEventsForJoystickState(EventDriverRef _Nonnull pDriver, const JoystickReport* pCurrent, const JoystickReport* pPrevious, Int portId, TimeInterval curTime)
{
    HIDEvent evt;
        
    // Generate joystick button up/down events
    const UInt32 old_buttons = pPrevious->buttonsDown;
    const UInt32 new_buttons = pCurrent->buttonsDown;

    if (new_buttons != old_buttons) {
        // XXX should be able to ask the joystick input driver how many buttons it supports
        for (Int i = 0; i < 2; i++) {
            const UInt32 old_down = old_buttons & (1 << i);
            const UInt32 new_down = new_buttons & (1 << i);
                
            if (old_down ^ new_down) {
                if (old_down == 0 && new_down != 0) {
                    evt.type = kHIDEventType_JoystickDown;
                } else {
                    evt.type = kHIDEventType_JoystickUp;
                }
                evt.eventTime = curTime;
                evt.data.joystick.port = portId;
                evt.data.joystick.buttonNumber = i;
                evt.data.joystick.flags = pDriver->key_modifierFlags;
                evt.data.joystick.direction.dx = pCurrent->xAbs;
                evt.data.joystick.direction.dy = pCurrent->yAbs;
                EventDriver_PutEvent_Locked(pDriver, &evt);
            }
        }
    }
    
    
    // Generate motion events
    const Int16 diffX = pCurrent->xAbs - pPrevious->xAbs;
    const Int16 diffY = pCurrent->yAbs - pPrevious->yAbs;
    
    if (diffX != 0 || diffY != 0) {
        evt.type = kHIDEventType_JoystickMotion;
        evt.eventTime = curTime;
        evt.data.joystickMotion.port = portId;
        evt.data.joystickMotion.direction.dx = pCurrent->xAbs;
        evt.data.joystickMotion.direction.dy = pCurrent->yAbs;
        EventDriver_PutEvent_Locked(pDriver, &evt);
    }
}

static void EventDriver_GenerateJoysticksEvents(EventDriverRef _Nonnull pDriver, TimeInterval curTime)
{
    for (Int i = 0; i < MAX_INPUT_CONTROLLER_PORTS; i++) {
        JoystickReport current;
        
        switch (pDriver->port[i].type) {
            case kInputControllerType_AnalogJoystick:
                AnalogJoystickDriver_GetReport(pDriver->port[i].u.analogJoystick.driver, &current);

                EventDriver_GenerateEventsForJoystickState(pDriver,
                                                           &current,
                                                           &pDriver->port[i].u.analogJoystick.previous,
                                                           i, curTime);
                pDriver->port[i].u.analogJoystick.previous = current;
                break;
                
            case kInputControllerType_DigitalJoystick:
                DigitalJoystickDriver_GetReport(pDriver->port[i].u.digitalJoystick.driver, &current);

                EventDriver_GenerateEventsForJoystickState(pDriver,
                                                           &current,
                                                           &pDriver->port[i].u.digitalJoystick.previous,
                                                           i, curTime);
                pDriver->port[i].u.digitalJoystick.previous = current;
                break;
                
            default:
                break;
        }
    }
}

static void EventDriver_Run(EventDriverRef _Nonnull pDriver)
{
    while(true) {
        BinarySemaphore_Acquire(&pDriver->vb_sema, kTimeInterval_Infinity);

        Lock_Lock(&pDriver->lock);
        const TimeInterval curTime = MonotonicClock_GetCurrentTime();

        // Process mouse input
        EventDriver_UpdateMouseState(pDriver);
        
        
        // Generate keyboard events
        EventDriver_GenerateKeyboardEvents(pDriver, curTime);
        
        // Generate mouse events
        EventDriver_GenerateMouseEvents(pDriver, curTime);
        
        // Generate joystick events
        EventDriver_GenerateJoysticksEvents(pDriver, curTime);
        
        
        // Wake up the event queue consumer
        ConditionVariable_Broadcast(&pDriver->event_queue_cv, &pDriver->lock);
    }
}
