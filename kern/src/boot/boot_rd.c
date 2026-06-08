//
//  boot_rd.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <string.h>
#include <driver/IOCatalog.h>
#include <driver/PlatformController.h>
#include <driver/pseudo/VDMDriver.h>
#include <handler/DriverHandler.h>
#include <kpi/smg.h>


// Checks whether the platform controller is able to provide a boot-able disk image
// for a ROM/RAM disk and creates a ROM/RAM disk with the name '/vd-bus/rd0' from
// that image if so. Otherwise does nothing.
void auto_discover_boot_rd(void)
{
    decl_try_err();
    const SMG_Header* _Nonnull smg_hdr = PlatformController_GetBootImage(gPlatformController);

    if (smg_hdr == NULL) {
        return;
    }


    // Open the VDM
    HandlerRef hVdm = NULL;
    try(IOCatalog_Open(gIOCatalog, "/vd-bus/self", O_RDWR, &hVdm));
    VDMDriverRef vdm = DriverHandler_GetDriverAs(hVdm, VDMDriver);


    // Create a RAM disk and copy the ROM disk image into it. We assume for now
    // that the disk image is exactly 64k in size.
    const char* dmg = ((const char*)smg_hdr) + smg_hdr->headerSize;
    int type;

    if ((smg_hdr->options & SMG_OPTION_READONLY) == SMG_OPTION_READONLY) {
        type = VDM_TYPE_RAM;
    }
    else {
        type = VDM_TYPE_REF_ROM;
    }

    try(VDMDriver_CreateDisk(vdm, type, "rd0", smg_hdr->blockSize, smg_hdr->physicalBlockCount, dmg));

catch:
    if (hVdm) {
        Handler_Shutdown(hVdm);
        Object_Release(hVdm);
    }
}
