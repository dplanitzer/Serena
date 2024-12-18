//
//  DiskController.h
//  kernel
//
//  Created by Dietmar Planitzer on 12/17/24.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef DiskController_h
#define DiskController_h

#include "RamFSContainer.h"
#include <filemanager/FileManager.h>
#include <filesystem/IOChannel.h>
#include <System/File.h>
#include <User.h>


typedef struct DiskController {
    RamFSContainerRef _Nonnull  fsContainer;
    FileManager                 fm;
} DiskController;
typedef DiskController* DiskControllerRef;


extern errno_t DiskController_CreateWithContentsOfPath(const char* _Nonnull path, DiskControllerRef _Nullable * _Nonnull pOutSelf);
extern void DiskController_Destroy(DiskControllerRef _Nullable self);

#endif /* DiskController_h */
