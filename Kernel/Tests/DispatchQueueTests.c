//
//  DispatchQueueTests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 5/3/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <Asserts.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <System/System.h>


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: DispatchAsync
////////////////////////////////////////////////////////////////////////////////

static void OnAsync(void* _Nonnull pValue)
{
    int val = (int)pValue;
    
    printf("%d\n", val);
    //Delay(TimeInterval_MakeSeconds(2));
    assertOK(DispatchQueue_DispatchAsync(kDispatchQueue_Main, OnAsync, (void*)(val + 1)));
}

void dq_async_test(int argc, char *argv[])
{
    assertOK(DispatchQueue_DispatchAsync(kDispatchQueue_Main, OnAsync, (void*)0));
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: DispatchAsyncAfter
////////////////////////////////////////////////////////////////////////////////

static void OnAsyncAfter(void* _Nonnull pValue)
{
    int val = (int)pValue;
    
    printf("%d\n", val);
    assertOK(DispatchQueue_DispatchAsyncAfter(kDispatchQueue_Main, TimeInterval_Add(MonotonicClock_GetTime(), TimeInterval_MakeMilliseconds(500)), OnAsyncAfter, (void*)(val + 1), 0));
}

void dq_async_after_test(int argc, char *argv[])
{
    assertOK(DispatchQueue_DispatchAsync(kDispatchQueue_Main, OnAsyncAfter, (void*)0));
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: DispatchSync
////////////////////////////////////////////////////////////////////////////////

static void OnSync(void* _Nonnull pValue)
{
    int val = (int)pValue;
    
    Delay(TimeInterval_MakeMilliseconds(500));
    printf("%d  (Queue: %d)\n", val, DispatchQueue_GetCurrent());
}

// XXX Note: you can not call this code from the main queue because it ends up
// XXX blocking on itself. This is expected behavior.
void dq_sync_test(int argc, char *argv[])
{
    int queue, i = 0;

    assertOK(DispatchQueue_Create(0, 4, kDispatchQoS_Utility, kDispatchPriority_Normal, &queue));
    while (true) {
        DispatchQueue_DispatchSync(queue, OnSync, (void*) i);
        puts("--------");
        i++;
    }
}


// XXX
// XXX Port these to user space (was written for kernel space originally)
// XXX

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

    for(int i = 0; i < pSysDesc->motherboard_ram.descriptor_count; i++) {
        print("start: %p, size: %zu,  type: %s\n",
              pSysDesc->motherboard_ram.descriptor[i].lower,
              pSysDesc->motherboard_ram.descriptor[i].upper - pSysDesc->motherboard_ram.descriptor[i].lower,
              (pSysDesc->motherboard_ram.descriptor[i].type == MEM_TYPE_UNIFIED_MEMORY) ? "Chip" : "Fast");
    }
    print("--------\n");
    
    const int boardCount = DriverManager_GetExpansionBoardCount(gDriverManager);
    if (boardCount > 0) {
        for(int i = 0; i < boardCount; i++) {
            const zorro_board_t board = DriverManager_GetExpansionBoardAtIndex(gDriverManager, i);

            print("start: 0x%p, psize: %u, lsize: %u\n", board.start, board.physical_size, board.logical_size);
            print("type: %s %s\n", board.type == BOARD_TYPE_RAM ? "RAM" : "I/O", board.bus == ZORRO_2_BUS ? "[Z2]" : "[Z3]");
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
