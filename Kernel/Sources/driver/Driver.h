//
//  Driver.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Driver_h
#define Driver_h

#include <klib/List.h>
#include <kobj/Object.h>

typedef enum DriverModel {
    kDriverModel_Sync,
    kDriverModel_Async
} DriverModel;

typedef enum DriverState {
    kDriverState_Stopped = 0,
    kDriverState_Running,
    kDriverState_Terminated
} DriverState;


// A driver object binds to and manages a device. A device is a piece of hardware
// that does some work. In the device tree, all driver objects are leaf nodes in
// the tree.
open_class(Driver, Object,
    ListNode                    childNode;
    List                        children;
    DispatchQueueRef _Nullable  dispatchQueue;
    int                         state;
    DriverId                    driverId;
);
open_class_funcs(Driver, Object,

    // Invoked as the result of calling Driver_Start(). A driver subclass should
    // override this method to reset the hardware, configure it such that
    // all components are in an idle state and to publish the driver to the
    // driver catalog.
    // Override: Recommended
    // Default Behavior: Returns EOK and does nothing
    errno_t (*start)(void* _Nonnull self);

    // Invoked as the result of calling Driver_Stop(). A driver subclass should
    // override this method and configure the hardware such that it is in an
    // idle and powered-down state.
    // Override: Optional
    // Default Behavior: Unpublishes the driver
    void (*stop)(void* _Nonnull self);

    // Invoked as the result of calling Driver_Open(). A driver subclass may
    // override this method to create a driver specific I/O channel object. The
    // default implementation creates a DriverChannel instance which supports
    // read, write, ioctl operations.
    // Override: Optional
    // Default Behavior: Creates a DriverChannel instance
    errno_t (*open)(void* _Nonnull self, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel);

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

    // Invoked as the result of calling Driver_Ioctl(). A driver subclass should
    // override this method to implement support for the ioctl() system call.
    // Override: Optional
    // Default Behavior: Returns ENOTIOCTLCMD
    errno_t (*ioctl)(void* _Nonnull self, int cmd, va_list ap);
);

extern errno_t Driver_Start(DriverRef _Nonnull self);
extern void Driver_Terminate(DriverRef _Nonnull self);

extern errno_t Driver_Open(DriverRef _Nonnull self, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel);
extern errno_t Driver_Close(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel);

extern errno_t Driver_Read(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead);
extern errno_t Driver_Write(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten);
extern errno_t Driver_Ioctl(DriverRef _Nonnull self, int cmd, va_list ap);

// Returns the driver's non-persistent driver ID. Note that this ID does not
// survive a system reboot. This ID is generated and assigned to the driver on
// the first start and remains stable after that until the system shuts down
// even if the driver goes through multiple publish/unpublish cycles in the
// meantime.
#define Driver_GetDriverId(__self) \
    ((DriverRef)__self)->driverId


//
// Subclassers
//

// Create a driver instance. 'model' defines the driver model. A driver which
// implements the kDriverModel_Async executes all of its operations on a driver
// specific serial dispatch queue and it supports plug & play hardware. A driver
// which implements the kDriverModel_Sync model executes all operations
// synchronously and it does not support plug & play hardware. 
#define Driver_Create(__className, __model, __pOutDriver) \
    _Driver_Create(&k##__className##Class, __model, (DriverRef*)__pOutDriver)

// Initializes a driver instance.
extern errno_t Driver_Init(DriverRef _Nonnull self, DriverModel model);

// Returns the driver's serial dispatch queue. Only call this if the driver is
// an asynchronous driver.
#define Driver_GetDispatchQueue(__self) \
    ((DriverRef)__self)->dispatchQueue


// Publishes the driver instance to the driver catalog with the given name.
extern errno_t Driver_Publish(DriverRef _Nonnull self, const char* name);

// Removes the driver instance from the driver catalog.
extern void Driver_Unpublish(DriverRef _Nonnull self);


// Adds the given driver as a child to the receiver.
extern void Driver_AddChild(DriverRef _Nonnull self, DriverRef _Nonnull pChild);

// Adds the given driver to the receiver as a child. Consumes the provided strong
// reference.
extern void Driver_AdoptChild(DriverRef _Nonnull self, DriverRef _Nonnull _Consuming pChild);

// Removes the given driver from teh receiver. The given driver has to be a child
// of the receiver.
extern void Driver_RemoveChild(DriverRef _Nonnull self, DriverRef _Nonnull pChild);



// Do not call directly. Use the Driver_Create() macro instead
extern errno_t _Driver_Create(Class* _Nonnull pClass, DriverModel model, DriverRef _Nullable * _Nonnull pOutDriver);

#endif /* Driver_h */
