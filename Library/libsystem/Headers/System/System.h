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
#include <System/Directory.h>
#include <System/Disk.h>
#include <System/Error.h>
#include <System/File.h>
#include <System/Filesystem.h>
#include <System/FilePermissions.h>
#include <System/Framebuffer.h>
#include <System/HID.h>
#include <System/HIDEvent.h>
#include <System/HIDKeyCodes.h>
#include <System/IOChannel.h>
#include <System/Lock.h>
#include <System/Pipe.h>
#include <System/Process.h>
#include <System/Semaphore.h>
#include <System/TimeInterval.h>
#include <System/Urt.h>
#include <System/User.h>
#include <System/Zorro.h>

__CPP_BEGIN

#if !defined(__KERNEL__)

// Initializes the libSystem services. This function must be called by the high-level
// language specific initialization code. Ie the C library invokes this function.
// Application developers do not need to call this function.
// @Concurrency: Not Safe
extern void System_Init(ProcessArguments* _Nonnull argsp);

// Shut down the boot screen and initialize the kerne VT100 console
// XXX for now. Don't use outside of login.
extern errno_t System_ConInit(void);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_SYSTEM_H */
