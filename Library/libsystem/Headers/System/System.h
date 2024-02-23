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
#include <System/Types.h>
#include <System/Clock.h>
#include <System/DispatchQueue.h>
#include <System/Error.h>
#include <System/File.h>
#include <System/IOChannel.h>
#include <System/Pipe.h>
#include <System/Process.h>
#include <System/TimeInterval.h>
#include <System/Urt.h>

__CPP_BEGIN

#if !defined(__KERNEL__)

extern void System_Init(ProcessArguments* _Nonnull argsp);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_SYSTEM_H */
