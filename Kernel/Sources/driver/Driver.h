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

typedef enum DriverModel {
    kDriverModel_Sync,
    kDriverModel_Async
} DriverModel;

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


// A driver object binds to and manages a device. A device is a piece of hardware
// while a driver is the software that manages the hardware.
//
// A driver may implement a synchronous or an asynchronous model. The driver
// model determines which kind of functionality the driver supports.
//
// A synchronous driver is a very simple kind of driver that is created and then
// stays alive until the system is shut down. It is not able to detect loss of
// hardware and it can not be terminated. A synchronous driver executes all
// driver functions immediately and in the same execution context as the calling
// function.
//
// An asynchronous driver is created and then stays alive until it is terminated.
// The termination function may be called at any time and it blocks the caller
// until the driver has finished shutting down. Terminating a driver relinquishes
// control of the hardware that the driver managed. Once Driver_Terminate()
// returns, a new driver instance of the same of a different class may be created
// and bound to the hardware.
// An asynchronous driver supports detecting loss of hardware. Meaning that the
// driver detects in the course of its normal operation that there is a problem
// with the hardware that it manages. Ie the hardware has lost power, has been
// disconnected from the bus to which it was connected or has become defective.
// An asynchronous driver executes all of its functionality on a driver-specific
// dispatch queue. Note that the functionality may either execute synchronously
// or asynchronously from the viewpoint of the code that invoked the
// functionality.
// 
// Driver Model Summary:
//
// Synchronous:
//    - lives until system shutdown
//    - does not support Driver_Terminate()
//    - executes all functions in the context of the calling execution thread
//    - all functions are executed synchronously from the viewpoint of the caller
//
// Asynchronous:
//     - lives until Driver_Terminate() is called
//     - supports Driver_Terminate()
//     - executes all functions as work items on its own dispatch queue
//     - driver functions may execute synchronously or asynchronously from the
//       viewpoint of the caller
// 
open_class(Driver, Object,
    Lock                        lock;
    ListNode                    childNode;
    List                        children;
    DispatchQueueRef _Nullable  dispatchQueue;
    int8_t                      state;
    int8_t                      model;
    uint8_t                     options;
    uint8_t                     flags;
    DriverCatalogId             driverCatalogId;
);
open_class_funcs(Driver, Object,

    // Invoked when the driver needs to create its dispatch queue. This will
    // only happen for an asynchronous driver.
    // Override: Optional
    // Default Behavior: create a dispatch queue with priority Normal
    errno_t (*createDispatchQueue)(void* _Nonnull self, DispatchQueueRef _Nullable * _Nonnull pOutQueue);

    
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
    void (*onStop)(void* _Nonnull self);


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
    errno_t (*createChannel)(void* _Nonnull self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel);

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
// Driver Model: All
extern errno_t Driver_Start(DriverRef _Nonnull self);

// Terminates the driver. This function blocks the caller until the termination
// has completed. Note that the termination will only complete after all still
// queued driver requested have finished executing.
// Driver Model: Asynchronous
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

// Create a driver instance. 'model' defines the driver model. A driver which
// implements the kDriverModel_Async executes all of its operations on a driver
// specific serial dispatch queue and it supports plug & play hardware. A driver
// which implements the kDriverModel_Sync model executes all operations
// synchronously and it does not support plug & play hardware. 
#define Driver_Create(__className, __model, __options, __pOutDriver) \
    _Driver_Create(&k##__className##Class, __model, __options, (DriverRef*)__pOutDriver)

// Initializes a driver instance.
extern errno_t Driver_Init(DriverRef _Nonnull self, DriverModel model, DriverOptions options);

// Returns the driver's serial dispatch queue. Only call this if the driver is
// an asynchronous driver.
#define Driver_GetDispatchQueue(__self) \
    ((DriverRef)__self)->dispatchQueue


// Returns true if the driver is in active state; false otherwise
#define Driver_IsActive(__self) \
(((DriverRef)__self)->state == kDriverState_Active ? true : false)


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



// Do not call directly. Use the Driver_Create() macro instead
extern errno_t _Driver_Create(Class* _Nonnull pClass, DriverModel model, DriverOptions options, DriverRef _Nullable * _Nonnull pOutDriver);

#endif /* Driver_h */
