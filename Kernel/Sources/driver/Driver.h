//
//  Driver.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Driver_h
#define Driver_h

#include <dispatcher/Lock.h>
#include <klib/List.h>
#include <kobj/Object.h>

typedef enum DriverOptions {
    kDriver_Exclusive = 1,    // At most one I/O channel can be open at any given time. Attempts to open more will generate a EBUSY error
    kDriver_Seekable = 2,       // Driver defines a seekable space and driver channel should allow seeking with the seek() system call
} DriverOptions;

typedef enum DriverState {
    kDriverState_Inactive = 0,
    kDriverState_Active,
    kDriverState_Terminating,
    kDriverState_Terminated
} DriverState;


// A driver object manages a device. A device is a piece of hardware while a
// driver is the software that manages the hardware.
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
// The Start(). Open(), Close() and Terminate() functions execute atomically
// with respect to each other. Ie an Open() call will not be interrupted by a
// Terminate() call.
//
// The driver Read(), Write() and Ioctl() functions are generically not expected
// to provide full atomicity since the driver channel class implements atomicity
// for those functions. However a driver subclass may have to implement some
// form of atomicity for read, write and ioctl to ensure that users using
// different driver channel at the same time can not inadvertently break the
// consistency of the hardware state.
//
// If a driver subclass introduces additional low-level functions that operate
// on a level below the driver channel and these functions are for consumption
// by other kernel components (ie DiskDriver.beginIO), then these functions must
// be protected by the driver lock (see Driver_Lock) to ensure that a
// termination can not happen in the middle of executing those low-level
// functions.
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
// An important advantage of this design where a Terminate() is only possible
// after all channels have been closed, is that the read, write and ioctl
// driver functions do not need to use the driver lock. They can implement their
// own kind of locking if really needed and otherwise just rely on the locking
// provided by the driver channel.
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
open_class(Driver, Object,
    Lock                    lock;
    ListNode/*<Driver>*/    childNode;
    List/*<Driver>*/        children;
    uint16_t                options;
    uint8_t                 flags;
    int8_t                  state;
    int                     openCount;
    DriverCatalogId         driverCatalogId;
    intptr_t                tag;
);
open_class_funcs(Driver, Object,
    
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


    // Invoked as part of the publishing the driver to the driver catalog. A
    // subclass may override this method to do extra work after the driver has
    // been published.
    // Override: Optional
    // Default behavior: does nothing
    errno_t (*onPublish)(void* _Nonnull _Locked self);

    // Invoked after the driver has been removed from the driver catalog. A
    // subclass may override this method to do extra work before the driver is
    // unpublished.
    // Override: Optional
    // Default behavior: does nothing
    void (*onUnpublish)(void* _Nonnull _Locked self);

   
    // Invoked as the result of calling Driver_Open(). A driver subclass may
    // override this method to create a driver specific I/O channel object. The
    // default implementation creates a DriverChannel instance which supports
    // read, write, ioctl operations.
    // Override: Optional
    // Default Behavior: Creates a DriverChannel instance
    errno_t (*open)(void* _Nonnull _Locked self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel);

    // Invoked by the open() function to create the driver channel that should
    // be returned to the caller.
    // Override: Optional
    // Default Behavior: returns a DriverChannel instance
    errno_t (*createChannel)(void* _Nonnull _Locked self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel);

    // Invoked as the result of calling Driver_Close().
    // Override: Optional
    // Default Behavior: Does nothing and returns EOK
    errno_t (*close)(void* _Nonnull _Locked self, IOChannelRef _Nonnull pChannel);


    // Invoked as the result of calling Driver_Read(). A driver subclass should
    // override this method to implement support for the read() system call.
    // Override: Optional
    // Default Behavior: Returns EBADF
    errno_t (*read)(void* _Nonnull self, IOChannelRef _Nonnull pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead);

    // Invoked as the result of calling Driver_Write(). A driver subclass should
    // override this method to implement support for the write() system call.
    // Override: Optional
    // Default Behavior: Returns EBADF
    errno_t (*write)(void* _Nonnull self, IOChannelRef _Nonnull pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten);

    // Invoked by the driver channel to get the size of the seekable space. The
    // maximum position to which a client is allowed to seek is this value minus
    // one.
    // Override: Optional
    // Default Behavior: Returns 0
    FileOffset (*getSeekableRange)(void* _Nonnull self);
    
    // Invoked as the result of calling Driver_Ioctl(). A driver subclass should
    // override this method to implement support for the ioctl() system call.
    // Override: Optional
    // Default Behavior: Returns ENOTIOCTLCMD
    errno_t (*ioctl)(void* _Nonnull self, int cmd, va_list ap);
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
invoke_n(read, Driver, __self, __pChannel, __pBuffer, __nBytesToRead, __nOutBytesRead)

#define Driver_Write(__self, __pChannel, __pBuffer, __nBytesToWrite, __nOutBytesWritten) \
invoke_n(write, Driver, __self, __pChannel, __pBuffer, __nBytesToWrite, __nOutBytesWritten)

extern errno_t Driver_Ioctl(DriverRef _Nonnull self, int cmd, ...);

#define Driver_vIoctl(__self, __cmd, __ap) \
invoke_n(ioctl, Driver, __self, __cmd, __ap)


// Set a tag on the driver. A tag is a value that a controller driver may assign
// to one of its child drivers. It can then look up a child based on its tag.
// Note that a tag must be set on a driver before its start() method is called
// and once set, the tag can not be changed anymore.
extern errno_t Driver_SetTag(DriverRef _Nonnull self, intptr_t tag);

// Returns the driver's tag. 0 is returned if the driver has no tag assigned to
// it.
extern intptr_t Driver_GetTag(DriverRef _Nonnull self);


//
// Subclassers
//

// Create a driver instance. 
#define Driver_Create(__className, __options, __pOutDriver) \
    _Driver_Create(&k##__className##Class, __options, (DriverRef*)__pOutDriver)


// Returns true if the driver is in active state; false otherwise
#define Driver_IsActive(__self) \
(((DriverRef)__self)->state == kDriverState_Active ? true : false)


// Locks the driver instance
#define Driver_Lock(__self) \
Lock_Lock(&((DriverRef)__self)->lock)

// Unlocks the driver instance
#define Driver_Unlock(__self) \
Lock_Unlock(&((DriverRef)__self)->lock)


#define Driver_OnStart(__self) \
invoke_0(onStart, Driver, __self)

#define Driver_OnStop(__self) \
invoke_0(onStop, Driver, __self)


#define Driver_OnPublish(__self) \
invoke_0(onPublish, Driver, __self)

#define Driver_OnUnpublish(__self) \
invoke_0(onUnpublish, Driver, __self)


// Publishes the driver instance to the driver catalog with the given name. This
// method should be called from a onStart() override.
extern errno_t Driver_Publish(DriverRef _Nonnull _Locked self, const char* _Nonnull name, intptr_t arg);

// Removes the driver instance from the driver catalog. Called as part of the
// driver termination process.
extern void Driver_Unpublish(DriverRef _Nonnull _Locked self);


// Creates an I/O channel that connects the driver to a user space application
// or a kernel space service
#define Driver_CreateChannel(__self, __mode, __arg, __pOutChannel) \
invoke_n(createChannel, Driver, __self, __mode, __arg, __pOutChannel)


// Returns the size of the seekable range
#define Driver_GetSeekableRange(__self) \
invoke_0(getSeekableRange, Driver, __self)


// Adds the given driver as a child to the receiver. Call this function from a
// onStart() override.
extern void Driver_AddChild(DriverRef _Nonnull _Locked self, DriverRef _Nonnull pChild);

// Adds the given driver to the receiver as a child. Consumes the provided strong
// reference. Call this function from a onStart() override.
extern void Driver_AdoptChild(DriverRef _Nonnull _Locked self, DriverRef _Nonnull _Consuming pChild);

// Removes the given driver from the receiver. The given driver has to be a child
// of the receiver. Call this function from a onStop() override.
extern void Driver_RemoveChild(DriverRef _Nonnull _Locked self, DriverRef _Nonnull pChild);

// Removes the first child driver with the tag 'tag'.
extern void Driver_RemoveChildWithTag(DriverRef _Nonnull _Locked self, intptr_t tag);

// Replaces the first child with the tag 'tag' with the new driver 'pNewChild'.
// Simply removes the existing child if 'pNewChild' is NULL.
extern void Driver_ReplaceChildWithTag(DriverRef _Nonnull _Locked self, intptr_t tag, DriverRef _Nonnull pNewChild);

// Returns a strong reference to the child driver with tag 'tag'. NULL is returned
// if no such child driver exists.
extern DriverRef _Nullable Driver_CopyChildWithTag(DriverRef _Nonnull _Locked self, intptr_t tag);


// Do not call directly. Use the Driver_Create() macro instead
extern errno_t _Driver_Create(Class* _Nonnull pClass, DriverOptions options, DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* Driver_h */
