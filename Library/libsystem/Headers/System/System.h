//
//  System.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/20/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_SYSTEM_H
#define _SYS_SYSTEM_H 1

#include <System/_cmndef.h>
#include <System/ByteOrder.h>
#include <System/Types.h>
#include <System/Clock.h>
#include <System/ConditionVariable.h>
#include <System/DispatchQueue.h>
#include <System/Error.h>
#include <System/Directory.h>
#include <System/Disk.h>
#include <System/File.h>
#include <System/FilePermissions.h>
#include <System/HID.h>
#include <System/IOChannel.h>
#include <System/Lock.h>
#include <System/Pipe.h>
#include <System/Process.h>
#include <System/Semaphore.h>
#include <System/TimeInterval.h>
#include <System/Urt.h>

__CPP_BEGIN

#if !defined(__KERNEL__)

// Initializes the libSystem services. This function must be called by the high-level
// language specific initialization code. Ie the C library invokes this function.
// Application developers do not need to call this function.
// @Concurrency: Not Safe
extern void System_Init(ProcessArguments* _Nonnull argsp);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_SYSTEM_H */
