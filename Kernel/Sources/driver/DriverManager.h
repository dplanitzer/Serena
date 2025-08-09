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
#include <kern/errno.h>
#include <kern/types.h>
#include <kobj/AnyRefs.h>



typedef struct DirEntry {
    CatalogId               dirId;
    const char* _Nonnull    name;
    uid_t                   uid;
    gid_t                   gid;
    mode_t                  perms;
} DirEntry;


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
extern errno_t DriverManager_Publish(DriverManagerRef _Nonnull self, const DriverEntry* _Nonnull de, did_t* _Nullable pOutId);

// Removes the driver instance from the driver catalog.
extern void DriverManager_Unpublish(DriverManagerRef _Nonnull self, did_t id);

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
extern errno_t DriverManager_CreateDirectory(DriverManagerRef _Nonnull self, const DirEntry* _Nonnull be, CatalogId* _Nonnull pOutDirId);

extern errno_t DriverManager_RemoveDirectory(DriverManagerRef _Nonnull self, CatalogId dirId);


// Called by a driver when it is ready for reaping.
extern void DriverManager_ReapDriver(DriverManagerRef _Nonnull self, DriverRef _Nonnull driver);

#endif /* DriverManager_h */
