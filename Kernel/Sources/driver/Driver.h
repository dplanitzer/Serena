//
//  Driver.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Driver_h
#define Driver_h

#include <stdarg.h>
#include <handler/Handler.h>
#include <klib/List.h>
#include <kpi/stat.h>
#include <kpi/uid.h>
#include <sched/mtx.h>
#include <Catalog.h>

enum {
    kDriver_Exclusive = 1,  // At most one I/O channel can be open at any given time. Attempts to open more will generate a EBUSY error
};

enum {
    kDriverState_Inactive = 0,
    kDriverState_Active,
    kDriverState_Terminating,
    kDriverState_Terminated
};


typedef enum IOCategory {
    kIOCategory_Keyboard = 1,
} IOCategory;


typedef struct DriverEntry {
    CatalogId               dirId;
    const char* _Nonnull    name;
    uid_t                   uid;
    gid_t                   gid;
    mode_t                  perms;
    IOCategory              category;
    HandlerRef _Nullable    driver;
    intptr_t                arg;
} DriverEntry;


// A driver object manages a device. A device is a piece of hardware while a
// driver is the software that manages the hardware.
//
//
// -- The Driver Lifecycle --
//
// A driver has a lifecycle:
// - create: driver was just created
// - active: entered by calling Driver_Start()
// - terminating: entered by calling Driver_Terminate()
//
// A driver must be started by calling Driver_Start() before any other driver
// function is called. It is however possible to release a driver reference by
// calling Object_Release() even before Driver_Start() is called.
//
// The Driver_Start() function transitions the driver lifecycle state to active
// and it invokes the onStart() method. A driver subclass is expected to override
// onStart() to publish the driver to the driver catalog by calling
// Driver_Publish(). Additionally the driver subclass can do device specific
// initialization work in onStart(). A driver will only enter active state if
// the onStart() override returns with EOK.
//
// Once a driver has been started, driver channels may be created by calling
// Driver_Open() and a driver channel should be closed by calling
// IOChannel_Close() on the channel. IOChannel_Close() in turn invokes
// Driver_Close().
//
// A driver may be voluntarily terminated by calling Driver_Terminate(). This
// function must be called before the last reference to the driver is released
// by calling Object_Release(). Note however that Driver_Terminate() will only
// terminate the driver if there are no more channels open. It returns EBUSY as
// long as there is at least one channel still open.
//
// A typical driver lifecycle looks like this:
//
// Driver_Create()
//   Driver_Start()
//     Driver_Open()
//       IOChannel_Read()
//       ...
//     Driver_Close()
//   Driver_Terminate()
// Object_Release()
//
// Note that I/O channels are really used in connection with drivers to track
// when a driver is in use. A driver can not be terminated while it is still
// being used by someone (a channel is still open). Thus you must access a
// driver through a channel.
//
//
// -- Drivers, Concurrency and Exclusivity --
//
// I/O channel provides important preconditions for what is discussed below:
// *) All operations on an I/O channel are executed serially
// *) At most one operation can be active on an I/O channel at any given time
// *) It guarantees that no operation is active when it calls Driver_Close()
// *) It guarantees that as soon as Driver_Close() starts executing and at any
//    time after it returns, no new I/O operations will be issued to the driver
//
// The driver operations start(), open(), close() and terminate() are exclusive
// to each other in terms concurrency. This means that only one of those
// operations will execute at any given time. This ensures for example that
// start() has completed before open() can begin execution and that open() has
// completed before close() can begin execution.
//
// The fact that all driver I/O operations (read, write, ioctl) require that the
// caller passes in a I/O channel ensures that none of these operations can be
// executed before open() has completed and returned a valid I/O channel.
//
// The fact that I/O channel guarantees that all active I/O operations on the
// channel have completed before it invokes close() on the driver ensures that
// close() can assume that no I/O operations can be active that are related to
// the channel that is passed to close(). Thus it is not necessary for close()
// to take the same lock that is used to protect the integrity of the I/O
// operations.
//
// Note that onStart(), onStop(), onOpen() and onClose() are invoked while the
// driver is holding the driver state management lock. This should not be of
// much relevance to a driver subclass since a driver subclass has no need to
// acquire its I/O operations lock from these overrides. See the previous
// paragraph for an explanation of why this is the case.
//
// A driver subclass is expected to guarantee that its read, write and ioctl
// operations are properly synchronized with each other and that invoking these
// methods concurrently will not lead to inconsistent hardware nor software
// state.
//
// A driver can achieve this by eg protecting these methods with an I/O
// operation lock (mutex), by using a serial dispatch queue or by implementing
// its own specialized I/O operations queueing mechanism.
//
//
// -- The Children of a Driver --
//
// A driver may create and manage child drivers. Child drivers are attached to
// their parent drivers and the parent driver maintains a strong reference to
// its child driver(s). This strong reference keeps a child driver alive as long
// as it remains attached to its parent.
//
// Note that if a child driver needs to use its parent driver to do its job,
// the child driver should receive a driver channel and use it. This allows the
// parent driver to properly track whether it is still in use or not (see
// Terminate()).
// 
// The parent-child driver relationship can be used to properly represent
// relationships like a bus and the devices on the bus. The bus is represented
// by the parent driver and each device on the bus is represented by a child
// driver.
//
// Another use case for the parent-child driver relationship is that of a multi-
// function expansion board: an expansion board which features a sound chip and
// a CD-ROM driver can be represented by a parent driver that manages the overall
// card functionality plus a child driver for the sound chip and another child
// driver for the CD-ROM drive.
//
open_class(Driver, Handler,
    mtx_t                   mtx;    // lifecycle management lock
    did_t                   id;     // unique id assigned at publish time
    CatalogId               parentDirectoryId;  // /dev directory in which the driver lives 
    List/*<Driver>*/        children;
    ListNode/*<Driver>*/    child_qe;
    uint16_t                options;
    uint8_t                 flags;
    int8_t                  state;  //XXX should be atomic_int
    int                     openCount;
    intptr_t                tag;
);
open_class_funcs(Driver, Handler,
    
    // Invoked as the result of calling Driver_Start(). A driver subclass should
    // override this method to reset the hardware, configure it such that
    // all components are in an idle state and to publish the driver to the
    // driver catalog.
    // Override: Recommended
    // Default Behavior: Returns EOK and does nothing
    errno_t (*onStart)(void* _Nonnull _Locked self);

    // Invoked as the result of calling Driver_Terminate(). A driver subclass
    // should override this method and configure the hardware such that it is in
    // an idle and powered-down state.
    // Override: Optional
    // Default Behavior: Unpublishes the driver
    void (*onStop)(void* _Nonnull _Locked self);

   
    // Invoked by the open() function to create the driver channel that should
    // be returned to the caller. The 'openCount' reflects the number of I/O
    // channels that are currently open for this driver. Note that this count
    // does not yet include the channel that should be created. A count of 0
    // indicates that this is the first channel that should be opened. A driver
    // subclass can use this information to eg power up the hardware if
    // necessary.
    // Override: Optional
    // Default Behavior: returns a HandlerChannel instance
    errno_t (*onOpen)(void* _Nonnull _Locked self, int openCount, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel);

    
    // Publish the driver. Should be called from the onStart() override.
    errno_t (*publish)(void* _Nonnull self, const DriverEntry* _Nonnull de);

    // Unpublishes the driver. Should be called from the onStop() override.
    void (*unpublish)(void* _Nonnull self);
);


// Starts the driver. This function must be called before any other driver
// function is called. It causes the driver to finish initialization and to
// publish its catalog entry to the driver catalog.
extern errno_t Driver_Start(DriverRef _Nonnull self);

// Terminates the driver. This function blocks the caller until the termination
// has completed. Note that the termination will only complete after all still
// queued driver requested have finished executing.
extern errno_t Driver_Terminate(DriverRef _Nonnull self);


// Opens an I/O channel to the driver with the mode 'mode'. EOK and the channel
// is returned in 'pOutChannel' on success and a suitable error code is returned
// otherwise.
extern errno_t Driver_Open(DriverRef _Nonnull self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Closes the given driver channel.
extern errno_t Driver_Close(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel);

#define Driver_Read(__self, __pChannel, __pBuffer, __nBytesToRead, __nOutBytesRead) \
Handler_Read(__self, __pChannel, __pBuffer, __nBytesToRead, __nOutBytesRead)

#define Driver_Write(__self, __pChannel, __pBuffer, __nBytesToWrite, __nOutBytesWritten) \
Handler_Write(__self, __pChannel, __pBuffer, __nBytesToWrite, __nOutBytesWritten)

#define Driver_vIoctl(__self, __chan, __cmd, __ap) \
Handler_vIoctl(__self, __chan, __cmd, __ap)


//
// Subclassers
//

// Creates a new driver instance.
extern errno_t Driver_Create(Class* _Nonnull pClass, unsigned options, CatalogId parentDirectoryId, DriverRef _Nullable * _Nonnull pOutSelf);


// Returns true if the driver is in active state; false otherwise
#define Driver_IsActive(__self) \
(((DriverRef)__self)->state == kDriverState_Active ? true : false)

// Returns the globally unique driver id. The id is assigned when the driver is
// published.
#define Driver_GetId(__self) \
((DriverRef)__self)->id

// Returns the parent directory of the driver. This is the directory in which
// the driver bus directories or the driver node itself lives.
#define Driver_GetParentDirectoryId(__self) \
((DriverRef)__self)->parentDirectoryId


// Publish the driver. Should be called from the onStart() override.
#define Driver_Publish(__self, __de) \
invoke_n(publish, Driver, __self, __de)

// Unpublishes the driver. Should be called from the onStop() override.
#define Driver_Unpublish(__self) \
invoke_0(unpublish, Driver, __self)


#define Driver_OnStart(__self) \
invoke_0(onStart, Driver, __self)

#define Driver_OnStop(__self) \
invoke_0(onStop, Driver, __self)


// Creates an I/O channel that connects the driver to a user space application
// or a kernel space service
#define Driver_OnOpen(__self, __openCount, __mode, __arg, __pOutChannel) \
invoke_n(onOpen, Driver, __self, __openCount, __mode, __arg, __pOutChannel)


// Adds the given driver as a child to the receiver. Call this function from a
// onStart() override.
extern void Driver_AddChild(DriverRef _Nonnull _Locked self, DriverRef _Nonnull pChild);

// Adds the given driver to the receiver as a child. Consumes the provided strong
// reference. Call this function from a onStart() override.
extern void Driver_AdoptChild(DriverRef _Nonnull _Locked self, DriverRef _Nonnull _Consuming pChild);

// Starts the given driver instance and adopts the driver instance as a child if
// the start has been successful.
extern errno_t Driver_StartAdoptChild(DriverRef _Nonnull _Locked self, DriverRef _Nonnull _Consuming pChild);

// Removes the given driver from the receiver. The given driver has to be a child
// of the receiver. Call this function from a onStop() override.
extern void Driver_RemoveChild(DriverRef _Nonnull _Locked self, DriverRef _Nonnull pChild);

// Returns a reference to the child driver with tag 'tag'. NULL is returned if
// no such child driver exists.
extern DriverRef _Nullable Driver_GetChildWithTag(DriverRef _Nonnull _Locked self, intptr_t tag);

#endif /* Driver_h */
