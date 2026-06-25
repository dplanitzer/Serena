//
//  IORegistry.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/24/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef IORegistry_h
#define IORegistry_h

#include <kobj/Object.h>
#include <kpi/ioctl.h>
#include <kpi/types.h>


#define IOMATCH_STARTED     1
#define IOMATCH_STOPPING    2

typedef void (*IOMatchCallback)(void* _Nullable arg, DriverRef _Nonnull driver, int notify);


extern IORegistryRef gIORegistry;


extern void IORegistry_Init(void);

extern void IORegistry_RegisterDriver(IORegistryRef _Nonnull self, DriverRef _Nonnull drv);
extern void IORegistry_DeregisterDriver(IORegistryRef _Nonnull self, DriverRef _Nonnull drv);


// Returns a snapshot of strong references to all drivers that match the provided
// categories. The caller is responsible for releasing all references and calling
// kfree() on the returned pointer when done. The array of driver references is
// terminated by a NULL entry.
extern errno_t IORegistry_CopyMatchingDrivers(IORegistryRef _Nonnull self, const iocat_t* _Nonnull cats, DriverRef* _Nullable * _Nonnull pOutDrivers);

// Same as above but return the best matching driver only. Returns ENODEV if
// no matching driver could be found.
extern DriverRef _Nullable IORegistry_CopyBestMatchingDriver(IORegistryRef _Nonnull self, const iocat_t* _Nonnull cats);

// Registers a continuous driver matcher with the IORegistry. This matcher
// will invoke 'f' with the argument 'arg' and the matching driver everytime a
// driver matching any of the I/O categories 'cats' is started or stopped.
// The function 'f' is also invoked with each driver that is already started
// at the time this function is called and that matches any of the I/O categories
// 'cats'. The matching stays in effect until it is cancelled by calling
// IORegistry_StopMatching().
// Note: the function 'f' is called while the IORegistry is locked. Thus this
// function will trigger a deadlock if it invokes any of the IORegistry methods.
extern errno_t IORegistry_StartMatching(IORegistryRef _Nonnull self, const iocat_t* _Nonnull cats, IOMatchCallback _Nonnull f, void* _Nullable arg);

// Cancels the driver matcher bound to the function 'f' and the argument 'arg'.
extern void IORegistry_StopMatching(IORegistryRef _Nonnull self, IOMatchCallback _Nonnull f, void* _Nullable arg);


// Opens the best driver that matches 'cats'.
extern errno_t IORegistry_OpenBestMatch(IORegistryRef _Nonnull self, const iocat_t* _Nonnull cats, fd_flags_t oflags, DriverRef _Nullable * _Nonnull pOutDriver);

#endif /* IORegistry_h */
