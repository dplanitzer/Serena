//
//  sys_desc.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <kern/kernlib.h>
#include <kern/string.h>
#include <machine/cpu.h>
#include <machine/sys_desc.h>
#include <machine/hw/m68k-amiga/chipset.h>


extern int8_t fpu_get_model(void);


sys_desc_t* _Nonnull g_sys_desc;


bool mem_size_region(void* _Nullable p0, void* _Nullable p1, size_t stepSize, int8_t type, mem_layout_t* _Nonnull pMemLayout)
{
    if (pMemLayout->desc_count >= MEM_DESC_CAPACITY) {
        return false;
    }

    mem_desc_t* pMemDesc = &pMemLayout->desc[pMemLayout->desc_count];
    bool okay = false;

    if (p0 < p1) {
        uint8_t* pLower = __Ceil_Ptr_PowerOf2((uint8_t*)p0, CPU_PAGE_SIZE);
        uint8_t* pUpper = __Floor_Ptr_PowerOf2((uint8_t*)p1, CPU_PAGE_SIZE);
        uint8_t* p = pLower;

        while (pUpper - p >= 4) {
            if (cpu_verify_ram_4b(p) != 0) {
                break;
            }

            p = __min(p + stepSize, (uint8_t*)p1);
        }

        if (p - pLower > 0) {
            pMemDesc->lower = pLower;
            pMemDesc->upper = p;
            pMemDesc->type = type;
            pMemLayout->desc_count++;
            okay = true;
        }
    }
    else if (p0 > p1) {
        uint8_t* pLower = __Ceil_Ptr_PowerOf2((uint8_t*)p1, CPU_PAGE_SIZE);
        uint8_t* pUpper = __Floor_Ptr_PowerOf2((uint8_t*)p0, CPU_PAGE_SIZE);
        uint8_t* p = pUpper - stepSize;

        while (p >= pLower && (p - pLower) >= 4) {
            if (cpu_verify_ram_4b(p) != 0) {
                p += stepSize;
                break;
            }

            p = __max(p - stepSize, (uint8_t*)p1);
        }

        if (pUpper - p > 0) {
            pMemDesc->lower = p;
            pMemDesc->upper = pUpper;
            pMemDesc->type = type;
            pMemLayout->desc_count++;
            okay = true;
        }
    }

    return okay;
}

// Invoked by the OnReset() function after the chipset has been reset. This
// function tests the motherboard RAM and figures out how much RAM is installed
// on the motherboard and which address ranges contain operating RAM chips.
static void mem_size_motherboard(sys_desc_t* pSysDesc, char* _Nullable pBootServicesMemoryTop)
{
    char* chip_ram_lower_p = pBootServicesMemoryTop;
    char* chip_ram_upper_p = pSysDesc->chipset_upper_dma_limit;

    // Forget the memory map set up in cpu_vectors_asm.s 'cause we'll build our own map here
    pSysDesc->motherboard_ram.desc_count = 0;
    
    
    // Memory map: http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node00D4.html
    
    // Scan chip RAM
    // 256KB chip memory (A1000)
    // 256KB chip memory (A500, A2000)
    // 512KB reserved if chipset limit < 1MB; otherwise 512KB chip memory (A2000)
    // 1MB reserved if chipset limit < 2MB; otherwise 1MB chip memory (A3000+)
    mem_size_region(chip_ram_lower_p, __min((char*)0x00200000, chip_ram_upper_p), SIZE_KB(256), MEM_TYPE_UNIFIED_MEMORY, &pSysDesc->motherboard_ram);
    
    
    // Scan expansion RAM (A500 / A2000 motherboard RAM)
    mem_size_region((char*)0x00c00000, (char*)0x00d80000, SIZE_KB(256), MEM_TYPE_MEMORY, &pSysDesc->motherboard_ram);
    
    
    // Scan 32bit (A3000 / A4000) motherboard RAM
    if (pSysDesc->chipset_ramsey_version > 0) {
        mem_size_region((char*)0x08000000, (char*)0x07000000, SIZE_MB(1), MEM_TYPE_MEMORY, &pSysDesc->motherboard_ram);
    }
}

static void ramsey_set_page_mode_enabled(bool flag)
{
    RAMSEY_BASE_DECL(cp);
    volatile uint8_t* p_cr = RAMSEY_REG_8(cp, RAMSEY_CR);
    uint8_t ref, r = *p_cr;

    if (flag) {
        r |= RAMSEY_CRF_PAGE_MODE;
        ref = 0;
    } else {
        r &= ~RAMSEY_CRF_PAGE_MODE;
        ref = RAMSEY_CRF_PAGE_MODE;
    }
    *p_cr = r;


    // Wait for the change to take effect
    do {
        r = *p_cr;
    } while ((r & RAMSEY_CRF_PAGE_MODE) == ref);
}

static bool mem_check_page_burst_compat(const mem_desc_t* _Nonnull pMemDesc, bool isA3000)
{
#define PAT0 0x5ac35ac3
#define PAT1 0xac35ac35
#define PAT2 0xc35ac35a
#define PAT3 0x35ac35ac
    volatile uint32_t* p = (volatile uint32_t*)pMemDesc->lower;
    const uint32_t* pu = (const uint32_t*)pMemDesc->upper;

    while (p < pu) {
        ramsey_set_page_mode_enabled(isA3000);
        p[0] = PAT0;
        p[1] = PAT1;
        p[2] = PAT2;
        p[3] = PAT3;
        ramsey_set_page_mode_enabled(!isA3000);
        
        if (p[0] != PAT0 || p[1] != PAT1 || p[2] != PAT2 || p[3] != PAT3) {
            return false;
        }

        p = (uint32_t*)((uint8_t*)p + SIZE_MB(1));
    }

    return true;
}

// Configures the RAM controller (RAMSEY). We check whether the motherboard 32bit
// fast RAM is compatible with page and burst mode and we'll turn those modes on
// if the RAM can handle it.
static void ramsey_configure(const sys_desc_t* _Nonnull pSysDesc)
{
    // Original A3000 and later A3000+ / A4000 designs use different RAM chips on
    // the motherboard that require different page mode compatibility checking
    // code.
    bool isA3000 = true;
    switch (pSysDesc->chipset_version) {
        case CHIPSET_8374_rev2_PAL:
        case CHIPSET_8374_rev2_NTSC:
        case CHIPSET_8374_rev3_PAL:
        case CHIPSET_8374_rev3_NTSC:
            isA3000 = false;
            break;
    }


    for (int i = 0; i < pSysDesc->motherboard_ram.desc_count; i++) {
        const mem_desc_t* pMemDesc = &pSysDesc->motherboard_ram.desc[i];

        if (pMemDesc->lower >= (char*)0x07000000 && pMemDesc->upper <= (char*)0x08000000) {
            if (!mem_check_page_burst_compat(pMemDesc, isA3000)) {
                return;
            }
        }
    }

    
    // Note that the refresh delay needs to be < 10us. However RAMSEY automatically
    // selects the right refresh mode by default. So we just leave the refresh
    // setting alone.
    RAMSEY_BASE_DECL(cp);
    volatile uint8_t* p_cr = RAMSEY_REG_8(cp, RAMSEY_CR);
    uint8_t r = *p_cr;

    r |= RAMSEY_CRF_PAGE_MODE;
    r |= RAMSEY_CRF_BURST_MODE;
    r &= ~RAMSEY_CRF_WRAP;  // Needs to be off for the 68040

    *p_cr = r;


    // Wait for the change to take effect
    do {
        r = *p_cr;
    } while ((r & RAMSEY_CRF_BURST_MODE) == 0);
}

static void gary_configure(void)
{
    GARY_BASE_DECL(cp);
    *GARY_REG_8(cp, GARY_COLDSTART) = *GARY_REG_8(cp, GARY_COLDSTART) & ~GARY_REGF_BIT;
    *GARY_REG_8(cp, GARY_TIMEOUT) = *GARY_REG_8(cp, GARY_TIMEOUT) | GARY_REGF_BIT;
}

// Initializes the system description which contains basic information about the
// platform. The system description is stored in low memory.
// \param pSysDesc the system description memory
// \param pBootServicesMemoryTop the end address of the memory used by the boot
//                               services. Range is [0...pBootServicesMemoryTop]
// \param cpu_model the detected CPU model 
void sys_desc_init(sys_desc_t* _Nonnull pSysDesc, char* _Nullable pBootServicesMemoryTop, int cpu_model)
{
    pSysDesc->cpu_model = cpu_model;
    pSysDesc->fpu_model = fpu_get_model();

    pSysDesc->chipset_version = (int8_t)chipset_get_version();
    pSysDesc->chipset_ramsey_version = (int8_t)chipset_get_ramsey_version();
    pSysDesc->chipset_upper_dma_limit = chipset_get_upper_dma_limit(pSysDesc->chipset_version);
        

    // Initialize Gary. We assume that Gary is around if Ramsey is around
    if (pSysDesc->chipset_ramsey_version > 0) {
        gary_configure();
    }


    // Find the populated motherboard RAM regions
    mem_size_motherboard(pSysDesc, pBootServicesMemoryTop);


    // Enable burst mode if possible (note 68020 doesn't support this)
    if (pSysDesc->chipset_ramsey_version > 0 && pSysDesc->cpu_model > CPU_MODEL_68020) {
        ramsey_configure(pSysDesc);
    }


    // Enable super scalar mode on the 68060
    if (pSysDesc->cpu_model == CPU_MODEL_68060) {
        cpu060_set_pcr_bits(M68060_PCR_ESS);
    }
}

// Returns the amount of physical RAM in the machine.
size_t sys_desc_getramsize(const sys_desc_t* _Nonnull self)
{
    size_t size = 0;

    for (int i = 0; i < self->motherboard_ram.desc_count; i++) {
        size += (self->motherboard_ram.desc[i].upper - self->motherboard_ram.desc[i].lower);
    }

    return size;
}
