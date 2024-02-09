//
//  SystemDescription.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "SystemDescription.h"
#include <klib/Bytes.h>


SystemDescription* _Nonnull gSystemDescription;


// Checks the physical CPU page that contains 'addr'. Returns true if the page
// exists and false if not.
static bool mem_probe_cpu_page(char* pAddr)
{
    char* pBaseAddr = __Floor_Ptr_PowerOf2(pAddr, CPU_PAGE_SIZE);
    char* pTopAddr = pBaseAddr + CPU_PAGE_SIZE - 8;
    char* pMiddleAddr = pBaseAddr + CPU_PAGE_SIZE / 2;

    if (cpu_verify_ram_4b(pBaseAddr)) {
        return false;
    }
    if (cpu_verify_ram_4b(pMiddleAddr)) {
        return false;
    }
    if (cpu_verify_ram_4b(pTopAddr)) {
        return false;
    }

    return true;
}

bool mem_check_region(MemoryLayout* pMemLayout, void* _Nullable lower, void* _Nullable upper, int8_t type)
{
    char* p = __Ceil_Ptr_PowerOf2(((char*)lower), CPU_PAGE_SIZE);
    char* pLimit = __Floor_Ptr_PowerOf2(((char*)upper), CPU_PAGE_SIZE);
    size_t nbytes = 0;
    bool hasMemory = false;
    bool hadMemory = false;
    
    if (pMemLayout->descriptor_count >= MEMORY_DESCRIPTORS_CAPACITY) {
        return false;
    }
    
    while (p < pLimit) {
        hasMemory = mem_probe_cpu_page(p) != 0;

        if (hasMemory) { nbytes += CPU_PAGE_SIZE; }
        
        if (!hadMemory && hasMemory) {
            // Transitioning from no memory to memory
            pMemLayout->descriptor[pMemLayout->descriptor_count].lower = p;
            pMemLayout->descriptor[pMemLayout->descriptor_count].upper = p;
            pMemLayout->descriptor[pMemLayout->descriptor_count].type = type;
        }
        
        if (hadMemory && !hasMemory) {
            // Transitioning from memory to no memory
            pMemLayout->descriptor[pMemLayout->descriptor_count].upper += nbytes;
            pMemLayout->descriptor_count++;
            nbytes = 0;
        }
        
        hadMemory = hasMemory;
        p += CPU_PAGE_SIZE;
    }
    
    if (hasMemory) {
        // We were scanning an existing memory region but we hit upperp. Close
        // the memory region.
        pMemLayout->descriptor[pMemLayout->descriptor_count].upper += nbytes;
        pMemLayout->descriptor_count++;
    }

    return true;
}

// Invoked by the OnReset() function after the chipset has been reset. This
// function tests the motherboard RAM and figures out how much RAM is installed
// on the motherboard and which address ranges contain operating RAM chips.
static void mem_check_motherboard(SystemDescription* pSysDesc, char* _Nullable pBootServicesMemoryTop)
{
    char* chip_ram_lower_p = pBootServicesMemoryTop;
    char* chip_ram_upper_p = pSysDesc->chipset_upper_dma_limit;

    // Forget the memory map set up in cpu_vectors_asm.s 'cause we'll build our own map here
    pSysDesc->memory.descriptor_count = 0;
    
    
    // Memory map: http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node00D4.html
    
    // Scan chip RAM
    // 256KB chip memory (A1000)
    // 256KB chip memory (A500, A2000)
    // 512KB reserved if chipset limit < 1MB; otherwise 512KB chip memory (A2000)
    // 1MB reserved if chipset limit < 2MB; otherwise 1MB chip memory (A3000+)
    mem_check_region(&pSysDesc->memory, chip_ram_lower_p, __min((char*)0x00200000, chip_ram_upper_p), MEM_TYPE_UNIFIED_MEMORY);
    
    
    // Scan expansion RAM (A500 / A2000 motherboard RAM)
    mem_check_region(&pSysDesc->memory, (char*)0x00c00000, (char*)0x00d80000, MEM_TYPE_MEMORY);
    
    
    // Scan 32bit (A3000 / A4000) motherboard RAM
    if (pSysDesc->chipset_ramsey_version > 0) {
        mem_check_region(&pSysDesc->memory, (char*)0x04000000, (char*)0x08000000, MEM_TYPE_MEMORY);
    }
}

extern int8_t fpu_get_model(void);

// Initializes the system description which contains basic information about the
// platform. The system description is stored in low memory.
// \param pSysDesc the system description memory
// \param pBootServicesMemoryTop the end address of the memory used by the boot
//                               services. Range is [0...pBootServicesMemoryTop]
// \param cpu_model the detected CPU model 
void SystemDescription_Init(SystemDescription* _Nonnull pSysDesc, char* _Nullable pBootServicesMemoryTop, int cpu_model)
{
    pSysDesc->cpu_model = cpu_model;
    pSysDesc->fpu_model = fpu_get_model();

    pSysDesc->chipset_version = (int8_t)chipset_get_version();
    pSysDesc->chipset_ramsey_version = (int8_t)chipset_get_ramsey_version();
    pSysDesc->chipset_upper_dma_limit = chipset_get_upper_dma_limit(pSysDesc->chipset_version);

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
    const bool is_ntsc = chipset_is_ntsc();
    
    pSysDesc->ns_per_quantum_timer_cycle = (is_ntsc) ? 1396 : 1409;
    pSysDesc->quantum_duration_cycles = (is_ntsc) ? 12000 : 12500;
    pSysDesc->quantum_duration_ns = (is_ntsc) ? 16761906 : 17621045;
    

    // Find the populated motherboard RAM regions
    mem_check_motherboard(pSysDesc, pBootServicesMemoryTop);
}
