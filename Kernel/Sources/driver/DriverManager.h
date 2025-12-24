//
//  DriverManager.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/1/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef DriverManager_h
#define DriverManager_h

#include <Catalog.h>
#include <driver/Driver.h>
#include <filemanager/ResolvedPath.h>
#include <kern/try.h>
#include <kern/types.h>
#include <kobj/AnyRefs.h>


typedef errno_t (*DriverManager_Iterator)(void* _Nullable arg, DriverRef _Nonnull driver, bool* _Nonnull pDone);


#define IONOTIFY_STARTED    1
#define IONOTIFY_STOPPING   2

typedef void (*drv_match_func_t)(void* _Nullable arg, DriverRef _Nonnull driver, int notify);


extern DriverManagerRef gDriverManager;


extern errno_t DriverManager_Create(DriverManagerRef _Nullable * _Nonnull pOutSelf);


// Returns the filesystem that represents the /dev catalog.
extern FilesystemRef _Nonnull DriverManager_GetCatalog(DriverManagerRef _Nonnull self);


// Returns a strong reference to the driver with the given driver id. Returns NULL
// if no such driver exists.
extern DriverRef _Nullable DriverManager_CopyDriverForId(DriverManagerRef _Nonnull self, did_t id);



extern errno_t DriverManager_Open(DriverManagerRef _Nonnull self, const char* _Nonnull path, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel);
extern errno_t DriverManager_HasDriver(DriverManagerRef _Nonnull self, const char* _Nonnull path);

extern errno_t DriverManager_AcquireNodeForPath(DriverManagerRef _Nonnull self, const char* _Nonnull path, ResolvedPath* _Nonnull rp);


// Publishes the driver instance to the driver catalog with the given name and
// returns the globally unique id that has been assigned to this driver
// publication. This method should be called from a Driver.onStart() override.
// Note that it is perfectly fine to publish the same driver instance multiple
// times as long as each instance is published under a different name. Each
// published instance is assigned a separate unique id.
extern errno_t DriverManager_CreateEntry(DriverManagerRef _Nonnull self, DriverRef _Nonnull drv, CatalogId parentDirId, const DriverEntry* _Nonnull de, did_t* _Nullable pOutId);

// Removes the driver instance from the driver catalog.
extern void DriverManager_RemoveEntry(DriverManagerRef _Nonnull self, did_t id);

// Publishes the receiver to the driver catalog as a bus owner and controller.
// This means that a directory is created in the driver catalog to represent the
// bus. The properties of this directory are defined by the bus entry 'be' struct.
// Then, optionally, a driver entry is published to the driver catalog as an
// immediate child of the bus directory. The properties of this driver entry are
// defined by the 'de' struct. The purpose of the driver entry is to allow a
// user space application to get information about the bus itself and to control
// various aspects of it. However a bus controller is not required to provide
// such an entry. The name of the controller driver entry should be "self".
// All children of the bus controller will be added to the bus directory of this
// controller.
extern errno_t DriverManager_CreateDirectory(DriverManagerRef _Nonnull self, CatalogId parentDirId, const DirEntry* _Nonnull be, CatalogId* _Nonnull pOutDirId);

extern errno_t DriverManager_RemoveDirectory(DriverManagerRef _Nonnull self, CatalogId dirId);


// Iterates through all currently published and started drivers and checks which
// ones match at least one of the I/O categories 'cats'. The array 'buf' is filled
// with strong references to the matching drivers. The number of driver references
// that fit in 'buf' is given by 'bufsiz'. The array is terminated by a NULL
// entry. Note that you are responsible for releasing the returned driver
// references once you no longer need them. EINVAL is returned if 'bufsiz' is 0.
// ERANGE is returned if there are more matching drivers than are able to fit in
// 'buf' including the terminating NULL entry.  
extern errno_t DriverManager_GetMatches(DriverManagerRef _Nonnull self, const iocat_t* _Nonnull cats, DriverRef* _Nonnull buf, size_t bufsiz);

// Registers a continuous driver matcher with the driver manager. This matcher
// will invoke 'f' with the argument 'arg' and the matching driver everytime a
// driver matching any of the I/O categories 'cats' is started or stopped.
// The function 'f' is also invoked with each driver that is already started
// at the time this function is called and that matches any of the I/O categories
// 'cats'. The matching stays in effect until it is cancelled by calling
// DriverManager_StopMatching().
// Note: the function 'f' is called while the driver manager is locked. Thus this
// function will trigger a deadlock if it invokes any of the driver manager
// methods.
extern errno_t DriverManager_StartMatching(DriverManagerRef _Nonnull self, const iocat_t* _Nonnull cats, drv_match_func_t _Nonnull f, void* _Nullable arg);

// Cancels the driver matcher bound to the function 'f' and the argument 'arg'.
extern void DriverManager_StopMatching(DriverManagerRef _Nonnull self, drv_match_func_t _Nonnull f, void* _Nullable arg);


// Called by a driver when it is has started. This will trigger registered
// matchers.
extern void DriverManager_OnDriverStarted(DriverManagerRef _Nonnull self, DriverRef _Nonnull driver);

// Called by a driver when it is beginning its stopping process. This will
// trigger registered matchers.
extern void DriverManager_OnDriverStopping(DriverManagerRef _Nonnull self, DriverRef _Nonnull driver);

#endif /* DriverManager_h */
