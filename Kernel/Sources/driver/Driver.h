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
// A driver implements an immediate synchronous model. This means that calls to
// start, read, write, ioctl and terminate execute immediately and atomically.
//
// A driver has to be started by calling Driver_Start() after it has been created
// and before any other function is called on the driver. The start function
// completes the driver initialization and publishes the driver to the driver
// catalog.
// Once started the read, write and ioctl functions may be called repeatedly in
// any order.
// A driver is stopped by calling the Driver_Terminate() method. This method
// blocks the caller until the driver and all its child drivers are terminated.
// A driver can not be restarted after it has been terminated. The only thing
// that can be done at this point is to release it by calling Object_Release(). 
open_class(Driver, Object,
    Lock                lock;
    ListNode            childNode;
    List/*<Driver>*/    children;
    uint16_t            options;
    uint8_t             flags;
    int8_t              state;
    DriverCatalogId     driverCatalogId;
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
    errno_t (*open)(void* _Nonnull self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel);

    // Invoked by the open() function to create the driver channel that should
    // be returned to the caller.
    // Override: Optional
    // Default Behavior: returns a DriverChannel instance
    errno_t (*createChannel)(void* _Nonnull _Locked self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel);

    // Invoked as the result of calling Driver_Close().
    // Override: Optional
    // Default Behavior: Does nothing and returns EOK
    errno_t (*close)(void* _Nonnull self, IOChannelRef _Nonnull pChannel);


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
extern void Driver_Terminate(DriverRef _Nonnull self);


// Opens an I/O channel to the driver with the mode 'mode'. EOK and the channel
// is returned in 'pOutChannel' on success and a suitable error code is returned
// otherwise.
#define Driver_Open(__self, __mode, __arg, __pOutChannel) \
invoke_n(open, Driver, __self, __mode, __arg, __pOutChannel)

// Closes the given driver channel.
#define Driver_Close(__self, __pChannel) \
invoke_n(close, Driver, __self, __pChannel)

#define Driver_Read(__self, __pChannel, __pBuffer, __nBytesToRead, __nOutBytesRead) \
invoke_n(read, Driver, __self, __pChannel, __pBuffer, __nBytesToRead, __nOutBytesRead)

#define Driver_Write(__self, __pChannel, __pBuffer, __nBytesToWrite, __nOutBytesWritten) \
invoke_n(write, Driver, __self, __pChannel, __pBuffer, __nBytesToWrite, __nOutBytesWritten)

extern errno_t Driver_Ioctl(DriverRef _Nonnull self, int cmd, ...);

#define Driver_vIoctl(__self, __cmd, __ap) \
invoke_n(ioctl, Driver, __self, __cmd, __ap)


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


#define Driver_Synchronized(__self, __closure) \
Driver_Lock(__self); \
if (!Driver_IsActive(__self)) { \
    Driver_Unlock(__self); \
    return ENODEV; \
} \
{ __closure } \
Driver_Unlock(__self)


// Do not call directly. Use the Driver_Create() macro instead
extern errno_t _Driver_Create(Class* _Nonnull pClass, DriverOptions options, DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* Driver_h */
