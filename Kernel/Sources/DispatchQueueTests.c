//
//  DispatchQueueTests.c
//  Apollo
//
//  Created by Dietmar Planitzer on 5/3/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Foundation.h"
#include "Console.h"
#include "DispatchQueue.h"
#include "EventDriver.h"
#include "Heap.h"
#include "MonotonicClock.h"
#include "Pipe.h"
#include "SystemGlobals.h"
#include "SystemDescription.h"
#include "RealtimeClock.h"


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: DispatchAsync/DispatchAsyncAfter
////////////////////////////////////////////////////////////////////////////////

#if 0
static void OnPrintClosure(Byte* _Nonnull pValue)
{
    Int val = (Int)pValue;
    
    print("%d\n", val);
    //VirtualProcessor_Sleep(TimeInterval_MakeSeconds(2));
    DispatchQueue_DispatchAsync(DispatchQueue_GetMain(), DispatchQueueClosure_Make(OnPrintClosure, (Byte*)(val + 1)));
    //DispatchQueue_DispatchAsyncAfter(DispatchQueue_GetMain(), TimeInterval_Add(MonotonicClock_GetCurrentTime(), TimeInterval_MakeMilliseconds(500)), DispatchQueueClosure_Make(OnPrintClosure, (Byte*)(val + 1)));
}

void DispatchQueue_RunTests(void)
{
    DispatchQueue_DispatchAsync(DispatchQueue_GetMain(), DispatchQueueClosure_Make(OnPrintClosure, (Byte*)0));
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: DispatchAsync/DispatchAsyncAfter in **User Space**
////////////////////////////////////////////////////////////////////////////////

#if 0
void DispatchQueue_RunTests(void)
{
    TimerRef pTimer;
    
    Timer_Create(kTimeInterval_Zero, TimeInterval_MakeMilliseconds(250), DispatchQueueClosure_MakeUser((Closure1Arg_Func)0xfe0000, (Byte*)0), &pTimer);
    DispatchQueue_DispatchTimer(DispatchQueue_GetMain(), pTimer);
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: DispatchSync
////////////////////////////////////////////////////////////////////////////////

#if 0
static void OnPrintClosure(Byte* _Nonnull pValue)
{
    Int val = (Int)pValue;
    
    VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(500));
    print("%d  (Queue: %p, VP: %p)\n", val, DispatchQueue_GetCurrent(), VirtualProcessor_GetCurrent());
}

// XXX Note: you can not call this code from the main queue because it ends up
// XXX blocking on itself. This is expected behavior.
void DispatchQueue_RunTests(void)
{
    DispatchQueueRef pQueue;
    DispatchQueue_Create(4, DISPATCH_QOS_UTILITY, 0, NULL, &pQueue);
    Int i = 0;

    while (true) {
        DispatchQueue_DispatchSync(pQueue, DispatchQueueClosure_Make(OnPrintClosure, (Byte*) i));
        print("--------\n");
        i++;
    }
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Repeating timer
////////////////////////////////////////////////////////////////////////////////


#if 0
struct State {
    TimerRef    timer;
    Int         value;
};

static void OnPrintClosure(Byte* _Nonnull pValue)
{
    struct State* pState = (struct State*)pValue;

    if (pState->value < 10) {
        print("%d\n", pState->value);
    } else {
        print("Cancelled\n");
        Timer_Cancel(pState->timer);
    }
    pState->value++;
}


void DispatchQueue_RunTests(void)
{
    struct State* pState;
    
    assert(kalloc(sizeof(struct State), &pState) == EOK);
    
    pState->timer = Timer_Create(TimeInterval_Add(MonotonicClock_GetCurrentTime(), TimeInterval_MakeSeconds(1)), TimeInterval_MakeSeconds(1), DispatchQueueClosure_Make(OnPrintClosure, (Byte*)pState));
    pState->value = 0;

    print("Repeating timer\n");
    DispatchQueue_DispatchTimer(DispatchQueue_GetMain(), pState->timer);
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: True Sleep time
////////////////////////////////////////////////////////////////////////////////

#if 0
void DispatchQueue_RunTests(void)
{
    const SystemDescription* pSysDesc = SystemDescription_GetShared();
    const SystemGlobals* pGlobals = SystemGlobals_Get();
    /*
    if (pGlobals->rtc) {
        GregorianDate date;
        Int i = 0;

        while (true) {
            RealtimeClock_GetDate(pGlobals->rtc, &date);
            print(" %d:%d:%d  %d/%d/%d  %d\n", date.hour, date.minute, date.second, date.month, date.day, date.year, date.dayOfWeek);
            print("%d\n", i);
            i++;
            VirtualProcessor_Sleep(TimeInterval_MakeSeconds(1));
            Console_ClearScreen(Console_GetMain());
        }
    } else {
        print("*** no RTC\n");
    }

    print("---------\n");
    */

    for(Int i = 0; i < pSysDesc->memory_descriptor_count; i++) {
        print("start: 0x%p, size: %u,  type: %s\n",
              pSysDesc->memory_descriptor[i].lower,
              pSysDesc->memory_descriptor[i].upper - pSysDesc->memory_descriptor[i].lower,
              (pSysDesc->memory_descriptor[i].accessibility & MEM_ACCESS_CHIPSET) ? "Chip" : "Fast");
    }
    print("--------\n");
    
    if (pSysDesc->expansion_board_count > 0) {
        for(Int i = 0; i < pSysDesc->expansion_board_count; i++) {
            print("start: 0x%p, psize: %u, lsize: %u\n", pSysDesc->expansion_board[i].start, pSysDesc->expansion_board[i].physical_size, pSysDesc->expansion_board[i].logical_size);
            print("type: %s %s\n", pSysDesc->expansion_board[i].type == EXPANSION_TYPE_RAM ? "RAM" : "I/O", pSysDesc->expansion_board[i].bus == EXPANSION_BUS_ZORRO_2 ? "[Z2]" : "[Z3]");
            print("slot: %d\n", pSysDesc->expansion_board[i].slot);
            print("manu: $%x, prod: $%x\n", pSysDesc->expansion_board[i].manufacturer, pSysDesc->expansion_board[i].product);
            print("ser#: $%x\n", pSysDesc->expansion_board[i].serial_number);
            
            print("---\n");
        }
    } else {
        print("No expansion boards found!\n");
    }
    while (true) { cpu_sleep(); }
/*
    const TimeInterval t_start = MonotonicClock_GetCurrentTime();
    VirtualProcessor_Sleep(TimeInterval_MakeMicroseconds(1*1000));
    //VirtualProcessor_Sleep(kTimeInterval_Infinity);
    const TimeInterval t_stop = MonotonicClock_GetCurrentTime();
    const TimeInterval t_delta = TimeInterval_Subtract(t_stop, t_start);
    print("t_delta: %dus\n", t_delta.nanoseconds / 1000);
    */
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Event loop
////////////////////////////////////////////////////////////////////////////////


#if 1
static void OnMainClosure(Byte* _Nonnull pValue)
{
    EventDriverRef pDriver = SystemGlobals_Get()->event_driver;
    
    print("Event loop\n");
    while (true) {
        HIDEvent evt;
        Int count = 1;
        
        const Int err = EventDriver_GetEvents(pDriver, &evt, &count, kTimeInterval_Infinity);
        
        switch (evt.type) {
            case kHIDEventType_KeyDown:
            case kHIDEventType_KeyUp:
                print("%s: $%hhx   flags: $%hhx  isRepeat: %s\n",
                      (evt.type == kHIDEventType_KeyUp) ? "KeyUp" : "KeyDown",
                      (Int)evt.data.key.keycode,
                      (Int)evt.data.key.flags, evt.data.key.isRepeat ? "true" : "false");
                break;
                
            case kHIDEventType_FlagsChanged:
                print("FlagsChanged: $%hhx\n", evt.data.flags.flags);
                break;
                
            case kHIDEventType_MouseUp:
            case kHIDEventType_MouseDown:
                print("%s: %d   (%d, %d)   flags: $%hhx\n",
                      (evt.type == kHIDEventType_MouseUp) ? "MouseUp" : "MouseDown",
                      evt.data.mouse.buttonNumber,
                      evt.data.mouse.location.x,
                      evt.data.mouse.location.y,
                      (Int)evt.data.mouse.flags);
                break;

            case kHIDEventType_MouseMoved:
                print("MouseMoved   (%d, %d)\n",
                      evt.data.mouseMoved.location.x,
                      evt.data.mouseMoved.location.y);
                break;
                
            default:
                print("*** unknown\n");
        }
    }
}


void DispatchQueue_RunTests(void)
{
    DispatchQueue_DispatchAsync(DispatchQueue_GetMain(), DispatchQueueClosure_Make(OnMainClosure, NULL));
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Pipe
////////////////////////////////////////////////////////////////////////////////


#if 0
static void OnReadFromPipe(Byte* _Nonnull pValue)
{
    PipeRef pipe = (PipeRef) pValue;
    ErrorCode err = EOK;
    Byte buf[16];
    
    print("reader: %d, pid: %d\n", Pipe_GetByteCount(pipe, kPipe_Reader), VirtualProcessor_GetCurrent()->vpid);
    while (true) {
        //VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(200));
        buf[0] = '\0';
        const Int nRead = Pipe_Read(pipe, &err, buf, sizeof(buf), true, kTimeInterval_Infinity);
        //print("\nmain: %d, read: %d", Pipe_GetByteCount(pipe, kPipe_Reader), nRead);
        buf[nRead] = '\0';
        print((const Character*)buf);
    }
}

static void OnWriteToPipe(Byte* _Nonnull pValue)
{
    PipeRef pipe = (PipeRef) pValue;
    ErrorCode err = EOK;
    
    print("writer: %d, pid: %d\n", Pipe_GetByteCount(pipe, kPipe_Writer), VirtualProcessor_GetCurrent()->vpid);
    
    while (true) {
        VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(20));
        const Int nWritten = Pipe_Write(pipe, &err, "\nHello", 6, true, kTimeInterval_Infinity);
        //print("\nbackground: %d, written: %d", Pipe_GetByteCount(pipe, kPipe_Writer), nWritten);
    }
}


void DispatchQueue_RunTests(void)
{
//    PipeRef pipe = Pipe_Create(PIPE_DEFAULT_BUFFER_SIZE);
    PipeRef pipe = Pipe_Create(4);
    DispatchQueueRef pUtilityQueue;
    DispatchQueue_Create(4, DISPATCH_QOS_UTILITY, 0, NULL, &pUtilityQueue);

    DispatchQueue_DispatchAsync(DispatchQueue_GetMain(), DispatchQueueClosure_Make(OnWriteToPipe, (Byte*)pipe));
    DispatchQueue_DispatchAsync(pUtilityQueue, DispatchQueueClosure_Make(OnReadFromPipe, (Byte*)pipe));

}
#endif
