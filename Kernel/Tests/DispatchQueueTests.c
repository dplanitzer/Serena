//
//  DispatchQueueTests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 5/3/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <System/System.h>

// XXX
// XXX Port these to user space (was written for kernel space originally)
// XXX

////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: DispatchAsync/DispatchAsyncAfter
////////////////////////////////////////////////////////////////////////////////

#if 0
static void OnPrintClosure(void* _Nonnull pValue)
{
    int val = (int)pValue;
    
    print("%d\n", val);
    //VirtualProcessor_Sleep(TimeInterval_MakeSeconds(2));
    DispatchQueue_DispatchAsync(kDispatchQueue_Main, DispatchQueueClosure_Make(OnPrintClosure, (void*)(val + 1)));
    //DispatchQueue_DispatchAsyncAfter(kDispatchQueue_Main, TimeInterval_Add(MonotonicClock_GetCurrentTime(), TimeInterval_MakeMilliseconds(500)), DispatchQueueClosure_Make(OnPrintClosure, (void*)(val + 1)));
}

void DispatchQueue_RunTests(void)
{
    DispatchQueue_DispatchAsync(kDispatchQueue_Main, DispatchQueueClosure_Make(OnPrintClosure, (void*)0));
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: DispatchSync
////////////////////////////////////////////////////////////////////////////////

#if 0
static void OnPrintClosure(void* _Nonnull pValue)
{
    int val = (int)pValue;
    
    VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(500));
    print("%d  (Queue: %p, VP: %p)\n", val, DispatchQueue_GetCurrent(), VirtualProcessor_GetCurrent());
}

// XXX Note: you can not call this code from the main queue because it ends up
// XXX blocking on itself. This is expected behavior.
void DispatchQueue_RunTests(void)
{
    DispatchQueueRef pQueue;
    DispatchQueue_Create(0, 4, DISPATCH_QOS_UTILITY, 0, gVirtualProcessorPool, NULL, &pQueue);
    int i = 0;

    while (true) {
        DispatchQueue_DispatchSync(pQueue, DispatchQueueClosure_Make(OnPrintClosure, (void*) i));
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
    int         value;
};

static void OnPrintClosure(void* _Nonnull pValue)
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
    
    try_bang(kalloc(sizeof(struct State), &pState));
    
    Timer_Create(TimeInterval_Add(MonotonicClock_GetCurrentTime(), TimeInterval_MakeSeconds(1)), TimeInterval_MakeSeconds(1), DispatchQueueClosure_Make(OnPrintClosure, pState), &pState->timer);
    pState->value = 0;

    print("Repeating timer\n");
    DispatchQueue_DispatchTimer(gMainDispatchQueue, pState->timer);
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: System info
////////////////////////////////////////////////////////////////////////////////

#if 0
void DispatchQueue_RunTests(void)
{
    const SystemDescription* pSysDesc = gSystemDescription;

    print("CPU: %s\n", cpu_get_model_name(pSysDesc->cpu_model));
    print("FPU: %s\n", fpu_get_model_name(pSysDesc->fpu_model));
    print("Chipset version: $%x\n", (uint32_t) pSysDesc->chipset_version);
    print("RAMSEY version: $%x\n", pSysDesc->chipset_ramsey_version);
    print("\n");
 
    //kalloc_dump();
    //print("\n");

    /*
    RealtimeClockRef pRealtimeClock = (RealtimeClockRef) DriverManager_GetDriverForName(gDriverManager, kRealtimeClockName);
    if (pRealtimeClock) {
        GregorianDate date;
        int i = 0;

        while (true) {
            RealtimeClock_GetDate(pRealtimeClock, &date);
            print(" %d:%d:%d  %d/%d/%d  %d\n", date.hour, date.minute, date.second, date.month, date.day, date.year, date.dayOfWeek);
            print("%d\n", i);
            i++;
            VirtualProcessor_Sleep(TimeInterval_MakeSeconds(1));
            print("\033[2J");    // clear screen
        }
    } else {
        print("*** no RTC\n");
    }

    print("---------\n");
    */

    for(int i = 0; i < pSysDesc->memory.descriptor_count; i++) {
        print("start: 0x%p, size: %u,  type: %s\n",
              pSysDesc->memory.descriptor[i].lower,
              pSysDesc->memory.descriptor[i].upper - pSysDesc->memory.descriptor[i].lower,
              (pSysDesc->memory.descriptor[i].type == MEM_TYPE_UNIFIED_MEMORY) ? "Chip" : "Fast");
    }
    print("--------\n");
    
    const int boardCount = DriverManager_GetExpansionBoardCount(gDriverManager);
    if (boardCount > 0) {
        for(int i = 0; i < boardCount; i++) {
            const ExpansionBoard board = DriverManager_GetExpansionBoardAtIndex(gDriverManager, i);

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
static void OnMainClosure(void* _Nonnull pValue)
{
    EventDriverRef pEventDriver;
    IOChannelRef pChannel;
    HIDEvent evt;
    User user = {kRootUserId, kRootGroupId};

    pEventDriver = DriverManager_GetDriverForName(gDriverManager, kEventsDriverName);
    assert(pEventDriver != NULL);
    assert(IOResource_Open(pEventDriver, NULL/*XXX*/, kOpen_Read, user, &pChannel) == EOK);

    print("Event loop\n");
    while (true) {
        ssize_t nBytesRead;
        const errno_t err = IOChannel_Read(pChannel, &evt, sizeof(evt), &nBytesRead);
        if (err != EOK) {
            abort();
        }
        
        switch (evt.type) {
            case kHIDEventType_KeyDown:
            case kHIDEventType_KeyUp:
                print("%s: $%hhx   flags: $%hhx  isRepeat: %s\n",
                      (evt.type == kHIDEventType_KeyUp) ? "KeyUp" : "KeyDown",
                      (int)evt.data.key.keyCode,
                      (int)evt.data.key.flags, evt.data.key.isRepeat ? "true" : "false");
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
                      (int)evt.data.mouse.flags);
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
    DispatchQueue_DispatchAsync(kDispatchQueue_Main, DispatchQueueClosure_Make(OnMainClosure, NULL));
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Pipe
////////////////////////////////////////////////////////////////////////////////


#if 0
static void OnReadFromPipe(void* _Nonnull pValue)
{
    PipeRef pipe = (PipeRef) pValue;
    char buf[16];
    
    const int nbytes = Pipe_GetNonBlockingReadableCount(pipe);
    print("reader: %d, pid: %d\n", nbytes, VirtualProcessor_GetCurrent()->vpid);
    while (true) {
        //VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(200));
        buf[0] = '\0';
        const int nRead = Pipe_Read(pipe, buf, sizeof(buf), true, kTimeInterval_Infinity);
        //nbytes = Pipe_GetNonBlockingReadableCount(pipe);
        //print("\nmain: %d, read: %d", nbytes, nRead);
        buf[nRead] = '\0';
        print((const char*)buf);
    }
}

static void OnWriteToPipe(void* _Nonnull pValue)
{
    PipeRef pipe = (PipeRef) pValue;
    
    const int nbytes = Pipe_GetNonBlockingWritableCount(pipe);
    print("writer: %d, pid: %d\n", nbytes, VirtualProcessor_GetCurrent()->vpid);
    
    while (true) {
        VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(20));
        const int nWritten = Pipe_Write(pipe, "\nHello", 6, true, kTimeInterval_Infinity);
        //nbytes = Pipe_GetNonBlockingWritableCount(pipe);
        //print("\nbackground: %d, written: %d", nbytes, nWritten);
    }
}


void DispatchQueue_RunTests(void)
{
    PipeRef pipe = NULL;

//  try_bang(Pipe_Create(PIPE_DEFAULT_BUFFER_SIZE, &pipe));
    try_bang(Pipe_Create(4, &pipe));
    DispatchQueueRef pUtilityQueue;
    DispatchQueue_Create(0, 4, DISPATCH_QOS_UTILITY, 0, gVirtualProcessorPool, NULL, &pUtilityQueue);

    DispatchQueue_DispatchAsync(kDispatchQueue_Main, DispatchQueueClosure_Make(OnWriteToPipe, pipe));
    DispatchQueue_DispatchAsync(pUtilityQueue, DispatchQueueClosure_Make(OnReadFromPipe, pipe));

}
#endif
