//
//  DispatchQueueTests.c
//  Apollo
//
//  Created by Dietmar Planitzer on 5/3/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Foundation.h"
#include "kalloc.h"
#include "DispatchQueue.h"
#include "DriverManager.h"
#include "EventDriver.h"
#include "MonotonicClock.h"
#include "Pipe.h"
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
    DispatchQueue_DispatchAsync(gMainDispatchQueue, DispatchQueueClosure_Make(OnPrintClosure, (Byte*)(val + 1)));
    //DispatchQueue_DispatchAsyncAfter(gMainDispatchQueue, TimeInterval_Add(MonotonicClock_GetCurrentTime(), TimeInterval_MakeMilliseconds(500)), DispatchQueueClosure_Make(OnPrintClosure, (Byte*)(val + 1)));
}

void DispatchQueue_RunTests(void)
{
    DispatchQueue_DispatchAsync(gMainDispatchQueue, DispatchQueueClosure_Make(OnPrintClosure, (Byte*)0));
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
    DispatchQueue_Create(4, DISPATCH_QOS_UTILITY, 0, gVirtualProcessorPool, NULL, &pQueue);
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
    
    try_bang(kalloc(sizeof(struct State), (Byte**)&pState));
    
    Timer_Create(TimeInterval_Add(MonotonicClock_GetCurrentTime(), TimeInterval_MakeSeconds(1)), TimeInterval_MakeSeconds(1), DispatchQueueClosure_Make(OnPrintClosure, (Byte*)pState), &pState->timer);
    pState->value = 0;

    print("Repeating timer\n");
    DispatchQueue_DispatchTimer(gMainDispatchQueue, pState->timer);
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: True Sleep time
////////////////////////////////////////////////////////////////////////////////

#if 1
void DispatchQueue_RunTests(void)
{
    const SystemDescription* pSysDesc = gSystemDescription;

    print("CPU: %s\n", cpu_get_model_name(pSysDesc->cpu_model));
    print("FPU: %s\n", fpu_get_model_name(pSysDesc->fpu_model));
    print("Chipset version: $%x\n", (UInt32) pSysDesc->chipset_version);
    print("RAMSEY version: $%x\n", pSysDesc->chipset_ramsey_version);
    print("\n");
 
    //kalloc_dump();
    //print("\n");

    /*
    RealtimeClockRef pRealtimeClock = (RealtimeClockRef) DriverManager_GetDriverForName(gDriverManager, kRealtimeClockName);
    if (pRealtimeClock) {
        GregorianDate date;
        Int i = 0;

        while (true) {
            RealtimeClock_GetDate(pRealtimeClock, &date);
            print(" %d:%d:%d  %d/%d/%d  %d\n", date.hour, date.minute, date.second, date.month, date.day, date.year, date.dayOfWeek);
            print("%d\n", i);
            i++;
            VirtualProcessor_Sleep(TimeInterval_MakeSeconds(1));
            print("\f");    // clear screen
        }
    } else {
        print("*** no RTC\n");
    }

    print("---------\n");
    */

    for(Int i = 0; i < pSysDesc->memory.descriptor_count; i++) {
        print("start: 0x%p, size: %u,  type: %s\n",
              pSysDesc->memory.descriptor[i].lower,
              pSysDesc->memory.descriptor[i].upper - pSysDesc->memory.descriptor[i].lower,
              (pSysDesc->memory.descriptor[i].type == MEM_TYPE_UNIFIED_MEMORY) ? "Chip" : "Fast");
    }
    print("--------\n");
    
    Int boardCount = 0;
    assert(DriverManager_GetExpansionBoardCount(gDriverManager, &boardCount) == EOK);
    if (boardCount > 0) {
        for(Int i = 0; i < boardCount; i++) {
            ExpansionBoard board;
            assert(DriverManager_GetExpansionBoardAtIndex(gDriverManager, i, &board) == EOK);

            print("start: 0x%p, psize: %u, lsize: %u\n", board.start, board.physical_size, board.logical_size);
            print("type: %s %s\n", board.type == EXPANSION_TYPE_RAM ? "RAM" : "I/O", board.bus == EXPANSION_BUS_ZORRO_2 ? "[Z2]" : "[Z3]");
            print("slot: %d\n", board.slot);
            print("manu: $%x, prod: $%x\n", board.manufacturer, board.product);
            print("ser#: $%x\n", board.serial_number);
            
            print("---\n");
        }
    } else {
        print("No expansion boards found!\n");
    }

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


#if 0
static void OnMainClosure(Byte* _Nonnull pValue)
{
    EventDriverRef pEventDriver = NULL;
    try_bang(DriverManager_GetDriverForName(gDriverManager, kEventsDriverName, (DriverRef*)&pEventDriver));

    print("Event loop\n");
    while (true) {
        HIDEvent evt;
        Int count = 1;
        
        const Int err = EventDriver_GetEvents(pEventDriver, &evt, &count, kTimeInterval_Infinity);
        
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
    DispatchQueue_DispatchAsync(gMainDispatchQueue, DispatchQueueClosure_Make(OnMainClosure, NULL));
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
    Int nbytes;
    
    Pipe_GetByteCount(pipe, kPipe_Reader, &nbytes);
    print("reader: %d, pid: %d\n", nbytes, VirtualProcessor_GetCurrent()->vpid);
    while (true) {
        //VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(200));
        buf[0] = '\0';
        Int nRead = 0;
        err = Pipe_Read(pipe, buf, sizeof(buf), true, kTimeInterval_Infinity, &nRead);
        //Pipe_GetByteCount(pipe, kPipe_Reader, &nbytes);
        //print("\nmain: %d, read: %d", nbytes, nRead);
        buf[nRead] = '\0';
        print((const Character*)buf);
    }
}

static void OnWriteToPipe(Byte* _Nonnull pValue)
{
    PipeRef pipe = (PipeRef) pValue;
    ErrorCode err = EOK;
    Int nbytes;
    
    Pipe_GetByteCount(pipe, kPipe_Writer, &nbytes);
    print("writer: %d, pid: %d\n", nbytes, VirtualProcessor_GetCurrent()->vpid);
    
    while (true) {
        VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(20));
        Int nWritten = 0;
        err = Pipe_Write(pipe, "\nHello", 6, true, kTimeInterval_Infinity, &nWritten);
        //Pipe_GetByteCount(pipe, kPipe_Writer, &nbytes);
        //print("\nbackground: %d, written: %d", nbytes, nWritten);
    }
}


void DispatchQueue_RunTests(void)
{
    PipeRef pipe = NULL;

//  try_bang(Pipe_Create(PIPE_DEFAULT_BUFFER_SIZE, &pipe));
    try_bang(Pipe_Create(4, &pipe));
    DispatchQueueRef pUtilityQueue;
    DispatchQueue_Create(4, DISPATCH_QOS_UTILITY, 0, gVirtualProcessorPool, NULL, &pUtilityQueue);

    DispatchQueue_DispatchAsync(gMainDispatchQueue, DispatchQueueClosure_Make(OnWriteToPipe, (Byte*)pipe));
    DispatchQueue_DispatchAsync(pUtilityQueue, DispatchQueueClosure_Make(OnReadFromPipe, (Byte*)pipe));

}
#endif
