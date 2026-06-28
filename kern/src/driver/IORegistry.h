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


typedef struct IOIterator {
    DriverRef* _Nonnull drivers;
    size_t              count;
    size_t              idx;
} IOIterator;

// Returns true if a call to IOIterator_GetNext() will return a driver reference
#define IOIterator_HasNext(__iter) \
((__iter)->idx < (__iter)->count)

// Returns the next driver reference and advances the iterator to the next entry.
// The returned reference is a weak reference. The caller must retain the driver
// if it wants to keep it past a IOIterator_Destroy() call
#define IOIterator_GetNext(__iter) \
(__iter)->drivers[(__iter)->idx++]

// Frees all iterator resources.
extern void IOIterator_Destroy(IOIterator* _Nullable iter);


extern IORegistryRef gIORegistry;


extern void IORegistry_Init(void);

extern void IORegistry_RegisterDriver(IORegistryRef _Nonnull self, DriverRef _Nonnull drv);
extern void IORegistry_DeregisterDriver(IORegistryRef _Nonnull self, DriverRef _Nonnull drv);


// Returns a strong reference to the driver with the driver ID 'id'. NULL is
// returned if no such driver exists.
extern DriverRef _Nullable IORegistry_CopyDriverWithId(IORegistryRef _Nonnull self, did_t id);


// Returns a snapshot of strong references to all drivers that match the provided
// categories. The caller is responsible for releasing all references and calling
// kfree() on the returned pointer when done. The array of driver references is
// terminated by a NULL entry.
extern errno_t IORegistry_CopyMatchingDrivers(IORegistryRef _Nonnull self, const iocat_t* _Nonnull cats, IOIterator* _Nonnull iter);

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
