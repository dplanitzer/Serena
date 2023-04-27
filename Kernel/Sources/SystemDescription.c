//
//  SystemDescription.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "SystemDescription.h"
#include "Bytes.h"
#include "Platform.h"


extern void zorro2_auto_config(SystemDescription* pSysDesc);
extern void zorro3_auto_config(SystemDescription* pSysDesc);


Bool mem_probe(Byte* addr)
{
    static const Byte* MEM_PATTERN = (const Byte*)"HbGtF1J8";
    Byte saved_bytes[8];
    Byte read_bytes[8];
    
    if (cpu_guarded_read(addr, saved_bytes, 8) != 0)    { return false; }
    if (cpu_guarded_write(addr, MEM_PATTERN, 8) != 0)   { return false; }
    if (cpu_guarded_read(addr, read_bytes, 8) != 0)     { return false; }
    if (cpu_guarded_write(addr, saved_bytes, 8) != 0)   { return false; }
    
    return Bytes_FindFirstDifference(read_bytes, MEM_PATTERN, 8) == -1;
}

static Bool mem_check_region(SystemDescription* pSysDesc, Byte* lower, Byte* upper, UInt8 accessibility, Int stepSize)
{
    Byte* p = lower;
    Int halfStepSize = stepSize / 2;
    UInt nbytes = 0;
    Bool hasMemory = false;
    Bool hadMemory = false;
    
    if (pSysDesc->memory_descriptor_count >= MEMORY_DESCRIPTORS_CAPACITY) {
        return false;
    }
    
    while (p < upper) {
        hasMemory = (mem_probe(p + halfStepSize) != 0);
        
        if (hasMemory) { nbytes += stepSize; }
        
        if (!hadMemory && hasMemory) {
            // Transitioning from no memory to memory
            pSysDesc->memory_descriptor[pSysDesc->memory_descriptor_count].lower = p;
            pSysDesc->memory_descriptor[pSysDesc->memory_descriptor_count].upper = p;
            pSysDesc->memory_descriptor[pSysDesc->memory_descriptor_count].accessibility = accessibility;
        }
        
        if (hadMemory && !hasMemory) {
            // Transitioning from memory to no memory
            pSysDesc->memory_descriptor[pSysDesc->memory_descriptor_count].upper += nbytes;
            pSysDesc->memory_descriptor_count++;
            nbytes = 0;
        }
        
        hadMemory = hasMemory;
        p += stepSize;
    }
    
    if (hasMemory) {
        // We were scanning an existing memory region but we hit upperp. Close
        // the memory region.
        pSysDesc->memory_descriptor[pSysDesc->memory_descriptor_count].upper += nbytes;
        pSysDesc->memory_descriptor_count++;
    }
    
    // Enforce the upper bound. We may have overshot it because 'lower' > 0 and
    // we scan memory in 'stepSize' increments.
    if (pSysDesc->memory_descriptor[pSysDesc->memory_descriptor_count - 1].upper > upper) {
        pSysDesc->memory_descriptor[pSysDesc->memory_descriptor_count - 1].upper = upper;
    }
    
    return true;
}

// Invoke by the Start() function after the chipset has been reset. This function
// tests the motherboard RAM and figures out how much RAM is installed on the
// motherboatd and which address ranges contain operating RAM chips.
void mem_check_motherboard(SystemDescription* pSysDesc)
{
    Byte* chip_ram_lower_p = pSysDesc->memory_descriptor[0].lower;  // set up by _Reset in traps.s
    Byte* chip_ram_upper_p = chipset_get_mem_limit();
    UInt step_size = 256 * 1024;    // Scan chip and motherboard RAM in 256KB steps
    Bool is32bit = cpu_is32bit();
    
    // Forget the memory map set up by traps.s 'cause we'll build our own map here
    pSysDesc->memory_descriptor_count = 0;
    
    
    // Memory map: http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node00D4.html
    
    // Scan chip RAM
    // 256KB chip memory (A1000)
    // 256KB chip memory (A500, A2000)
    // 512KB reserved if chipset limit < 1MB; otherwise 512KB chip memory (A2000)
    // 1MB reserved if chipset limit < 2MB; otherwise 1MB chip memory (A3000+)
    mem_check_region(pSysDesc, chip_ram_lower_p, min((Byte*)0x00200000, chip_ram_upper_p), MEM_ACCESS_CPU | MEM_ACCESS_CHIPSET, step_size);
    
    
    // Scan expansion RAM (A500 / A2000 motherboard RAM)
    mem_check_region(pSysDesc, (Byte*)0x00c00000, (Byte*)0x00d80000, MEM_ACCESS_CPU, step_size);
    
    
    // Scan 32bit (A3000 / A4000) motherboard RAM
    if (is32bit && pSysDesc->chipset_ramsey_version > 0) {
        mem_check_region(pSysDesc, (Byte*)0x04000000, (Byte*)0x08000000, MEM_ACCESS_CPU, step_size);
    }
}

// Finds out how much RAM is installed in expansion boards, tests it and adds it
// to the mem range table.
void mem_check_expanion_boards(SystemDescription* pSysDesc)
{
    Int step_size = 256 * 1024;    // Scan chip and motherboard RAM in 256KB steps
    
    for (Int i = 0; i < pSysDesc->expansion_board_count; i++) {
        const ExpansionBoard* board = &pSysDesc->expansion_board[i];
        
        if (board->type != EXPANSION_TYPE_RAM) {
            continue;
        }
        
        if (!mem_check_region(pSysDesc, board->start, board->start + board->size, MEM_ACCESS_CPU, step_size)) {
            break;
        }
    }
}



void SystemDescription_Init(SystemDescription* pSysDesc)
{
    volatile UInt8* pRAMSEY = (volatile UInt8*)RAMSEY_CHIP_BASE;
    
    pSysDesc->cpu_model = cpu_get_model();
    pSysDesc->fpu_model = fpu_get_model();
    pSysDesc->chipset_version = (Int8)chipset_get_version();
    
    // Figure out whether a RAMSEY chip is available
    pSysDesc->chipset_ramsey_version = *pRAMSEY;
    switch (pSysDesc->chipset_ramsey_version) {
        case CHIPSET_RAMSEY_rev04:
        case CHIPSET_RAMSEY_rev07:
            break;
            
        default:
            pSysDesc->chipset_ramsey_version = 0;
            break;
    }

    // Compute the quantum timer parameters:
    //
    // Amiga system clock:
    //  NTSC    28.63636 MHz
    //  PAL     28.37516 MHz
    //
    // CIA B timer A clock:
    //   NTSC    0.715909 MHz (1/10th CPU clock)     [1.3968255 us]
    //   PAL     0.709379 MHz                        [1.4096836 us]
    //
    // Quantum duration:
    //   NTSC    16.761906 ms    [12000 timer clock cycles]
    //   PAL     17.621045 ms    [12500 timer clock cycles]
    //
    // The quantum duration is chosen such that:
    // - it is approx 16ms - 17ms
    // - the value is a positive integer in terms of nanoseconds to avoid accumulating / rounding errors as time progresses
    //
    // The ns_per_quantum_timer_cycle value is rounded such that:
    // ns_per_quantum_timer_cycle * quantum_duration_cycles <= quantum_duration_ns
    // to ensure that we do not end up in a situation where the result of this product would return a quantum duration in
    // nanoseconds that's longer than what chipset_get_quantum_timer_duration_ns() returns.
    const Bool is_ntsc = chipset_is_ntsc();
    
    pSysDesc->ns_per_quantum_timer_cycle = (is_ntsc) ? 1396 : 1409;
    pSysDesc->quantum_duration_cycles = (is_ntsc) ? 12000 : 12500;
    pSysDesc->quantum_duration_ns = (is_ntsc) ? 16761906 : 17621045;
    
    
    // Probe RAM installed on the motherboard
    mem_check_motherboard(pSysDesc);

    
    // Auto config the Zorro bus
    if (pSysDesc->chipset_ramsey_version > 0) {
        zorro3_auto_config(pSysDesc);
    } else {
        zorro2_auto_config(pSysDesc);
    }

    
    // Find and add expansion board RAM
    // XXX disabled for now because adding expansion RAM causes boot to hang
    //mem_check_expanion_boards(pSysDesc);
}
