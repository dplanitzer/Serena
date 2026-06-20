//
//  IOIOCatalog.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef IOIOCatalog_h
#define IOIOCatalog_h

#include <kobj/Object.h>
#include <kpi/fs_perms.h>
#include <kpi/ioctl.h>
#include <kpi/types.h>
#include <filemanager/ResolvedPath.h>


typedef struct DirEntry {
    const char* _Nonnull    name;
    uid_t                   uid;
    gid_t                   gid;
    fs_perms_t              perms;
} DirEntry;


typedef struct DriverEntry {
    const char* _Nonnull    name;
    uid_t                   uid;
    gid_t                   gid;
    fs_perms_t              perms;
    intptr_t                arg;
} DriverEntry;


#define IONOTIFY_STARTED    1
#define IONOTIFY_STOPPING   2

typedef void (*drv_match_func_t)(void* _Nullable arg, DriverRef _Nonnull driver, int notify);


// The non-persistent, globally unique ID of a published catalog entry.
// This ID does not survive a system reboot. Id 0 represents a catalog
// entry that does not exist.
typedef uint32_t    CatalogId;

// Means no catalog entry
#define kCatalogId_None   0


extern IOCatalogRef gIOCatalog;


extern errno_t IOCatalog_Create(IOCatalogRef _Nullable * _Nonnull pOutSelf);

extern FilesystemRef _Nonnull IOCatalog_GetFilesystem(IOCatalogRef _Nonnull self);

// Looks up the inode for the given path and returns it.
extern errno_t IOCatalog_AcquireNodeForPath(IOCatalogRef _Nonnull self, const char* _Nonnull path, ResolvedPath* _Nonnull rp);

// Returns the inode that represents the given driver. A suitable error is
// returned if the driver has no inode associated with it, etc. You must call
// Inode_Relinquish() when you no longer need the inode.
extern errno_t IOCatalog_AcquireNodeForDriver(IOCatalogRef _Nonnull self, DriverRef _Nonnull driver, InodeRef _Nullable * _Nonnull pOutNode);

// Opens the catalog entry at the in-kernel path 'path' with mode 'mode' and
// returns the resulting channel in 'pOutHandler'. This call does not support
// opening a folder.
extern errno_t IOCatalog_Open(IOCatalogRef _Nonnull self, const char* _Nonnull path, unsigned int mode, HandlerRef _Nullable * _Nonnull pOutHandler);

// Opens the best driver that matches 'cats'.
extern errno_t IOCatalog_OpenBestMatch(IOCatalogRef _Nonnull self, const iocat_t* _Nonnull cats, unsigned int mode, DriverRef _Nullable * _Nonnull pOutDriver);


// Publishes a folder with the name 'name' to the catalog. Pass kIOCatalog_None as
// the 'parentFolderId' to create the new folder inside the root folder. 
extern errno_t IOCatalog_PublishFolder(IOCatalogRef _Nonnull self, CatalogId parentFolderId, const DirEntry* _Nonnull be, CatalogId* _Nonnull pOutFolderId);


// Either removes a published entry or a published folder from the catalog.
// Pass both a folder ID and the entry ID if you want to remove an entry.
// Note that this removes just the entry and not the published folder. Pass a
// folder ID and kIOCatalog_None as the entry ID to remove a folder. Note that the
// folder must be empty in order to remove it.
extern errno_t IOCatalog_Unpublish(IOCatalogRef _Nonnull self, CatalogId folderId, CatalogId entryId);


// Publish the driver instance 'drv' with the name 'name' as a child of the
// directory 'folderId', in the driver catalog. The driver is published as a
// child of the root directory if 'folderId' is kCatalogId_None.
// Returns a suitable error if another entry with the same name already exists.
// 'arg' is an optional argument that will be passed to the Driver_Open() method
// when the driver needs to be opened.
extern errno_t IOCatalog_PublishDriver(IOCatalogRef _Nonnull self, DriverRef _Nonnull drv, CatalogId folderId, const DriverEntry* _Nonnull de, did_t* _Nullable pOutId);


// Returns a strong reference to the driver with ID 'id'. Returns NULL and a
// suitable error in the case of a lookup problem.
extern errno_t IOCatalog_CopyDriverForId(IOCatalogRef _Nonnull self, CatalogId id, DriverRef _Nullable * _Nonnull pOutDriver);

// Returns a snapshot of strong references to all drivers that match the provided
// categories. The caller is responsible for releasing all references and calling
// kfree() on the returned pointer when done. The array of driver references is
// terminated by a NULL entry.
extern errno_t IOCatalog_CopyMatchingDrivers(IOCatalogRef _Nonnull self, const iocat_t* _Nonnull cats, DriverRef* _Nullable * _Nonnull pOutDrivers);

// Same as above but return the best matching driver only. Returns ENODEV if
// no matching driver could be found.
extern DriverRef _Nullable IOCatalog_CopyBestMatchingDriver(IOCatalogRef _Nonnull self, const iocat_t* _Nonnull cats);

// Registers a continuous driver matcher with the driver manager. This matcher
// will invoke 'f' with the argument 'arg' and the matching driver everytime a
// driver matching any of the I/O categories 'cats' is started or stopped.
// The function 'f' is also invoked with each driver that is already started
// at the time this function is called and that matches any of the I/O categories
// 'cats'. The matching stays in effect until it is cancelled by calling
// IOCatalog_StopMatching().
// Note: the function 'f' is called while the driver manager is locked. Thus this
// function will trigger a deadlock if it invokes any of the driver manager
// methods.
extern errno_t IOCatalog_StartMatching(IOCatalogRef _Nonnull self, const iocat_t* _Nonnull cats, drv_match_func_t _Nonnull f, void* _Nullable arg);

// Cancels the driver matcher bound to the function 'f' and the argument 'arg'.
extern void IOCatalog_StopMatching(IOCatalogRef _Nonnull self, drv_match_func_t _Nonnull f, void* _Nullable arg);


// Called by a driver when it is has started. This will trigger registered
// matchers.
extern void IOCatalog_OnDriverStarted(IOCatalogRef _Nonnull self, DriverRef _Nonnull driver);

// Called by a driver when it is beginning its stopping process. This will
// trigger registered matchers.
extern void IOCatalog_OnDriverStopping(IOCatalogRef _Nonnull self, DriverRef _Nonnull driver);

#endif /* IOIOCatalog_h */
