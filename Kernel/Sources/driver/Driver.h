//
//  Driver.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Driver_h
#define Driver_h

#include <kobj/Object.h>

typedef enum DriverModel {
    kDriverModel_Sync,
    kDriverModel_Async
} DriverModel;


// A driver object binds to and manages a device. A device is a piece of hardware
// that does some work. In the device tree, all driver objects are leaf nodes in
// the tree.
open_class(Driver, Object,
    DispatchQueueRef _Nullable  dispatchQueue;
    uint16_t                    options;
    AtomicBool                  isStopped;
);
open_class_funcs(Driver, Object,

    // Invoked as the result of calling Driver_Start(). A driver subclass should
    // override this method to reset the hardware and to configure it such that
    // all components are in an idle state.
    // Override: Optional
    // Default Behavior: Returns EOK and does nothing
    errno_t (*start)(void* _Nonnull self);

    // Invoked as the result of calling Driver_Stop(). A driver subclass should
    // override this method and configure the hardware such that it is in an
    // idle and powered-down state.
    // Override: Optional
    // Default Behavior: Does nothing
    void (*stop)(void* _Nonnull self);

    // Invoked as the result of calling Driver_Open(). A driver subclass may
    // override this method to create a driver specific I/O channel object. The
    // default implementation creates a DriverChannel instance which supports
    // read, write, ioctl operations.
    // Override: Optional
    // Default Behavior: Creates a DriverChannel instance
    errno_t (*open)(void* _Nonnull self, const char* _Nonnull path, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel);

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
extern void Driver_Stop(DriverRef _Nonnull self, bool waitForCompletion);

extern errno_t Driver_Open(DriverRef _Nonnull self, const char* _Nonnull path, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel);
extern errno_t Driver_Close(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel);

extern errno_t Driver_Read(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead);
extern errno_t Driver_Write(DriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten);
extern errno_t Driver_Ioctl(DriverRef _Nonnull self, int cmd, va_list ap);

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
extern errno_t Driver_Init(DriverRef _Nonnull self, DriverModel model, unsigned int options);

// Returns the driver's serial dispatch queue. Only call this if the driver is
// an asynchronous driver.
#define Driver_GetDispatchQueue(__self) \
    ((DriverRef)__self)->dispatchQueue
    

// Do not call directly. Use the Driver_Create() macro instead
extern errno_t _Driver_Create(Class* _Nonnull pClass, DriverModel model, unsigned int options, DriverRef _Nullable * _Nonnull pOutDriver);

#endif /* Driver_h */
