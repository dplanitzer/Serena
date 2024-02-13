//
//  apollo.h
//  libsystem
//
//  Created by Dietmar Planitzer on 10/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_APOLLO_H
#define _SYS_APOLLO_H 1

#include <apollo/_cmndef.h>
#include <apollo/types.h>
#include <apollo/DispatchQueue.h>
#include <apollo/Error.h>
#include <apollo/File.h>
#include <apollo/IOChannel.h>
#include <apollo/Process.h>
#include <apollo/Urt.h>

__CPP_BEGIN

#if !defined(__KERNEL__)

extern void System_Init(ProcessArguments* _Nonnull argsp);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_APOLLO_H */
