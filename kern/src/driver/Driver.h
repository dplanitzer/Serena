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
#include <ext/queue.h>
#include <kobj/Object.h>
#include <kern/devfs.h>
#include <kpi/file.h>
#include <kpi/ioctl.h>
#include <kpi/uid.h>
#include <sched/mtx.h>


// Driver instantiation option. Used by subclassers to request specific default
// behavior from the Driver class.
#define kDriver_Exclusive       1   // At most one I/O handler can be open at any given time. Attempts to open more will generate a EBUSY error


// INTERNAL
#define kDriverFlag_IsActive    1


// A driver object manages a device. A device is a piece of hardware while a
// driver is the software that manages the hardware.
//
open_class(Driver, Object,
    deque_node_t                ioreg_qe;   // dequeue is owned and managed by IORegistry
    DriverRef _Nullable         provider;   // strong ref to the provider driver; immutable between start() and stop()
    did_t                       id;         // globally unique driver id (> 0)

    mtx_t                       mtx;        // lifecycle management lock
    const iocat_t* _Nonnull     cats;       // categories the driver conforms to.
    devfs_hnd_t                 devfs_hnd;

    int                         openCount;
    uint16_t                    options;
    uint16_t                    flags;
);
open_class_funcs(Driver, Object,
    
    // Invoked when a driver is made active. Subclasses should override this
    // method, reset the hardware and/or establish the basic hardware
    // configuration and allocate resources that will be needed whether a client
    // is actually using the driver or not.
    // Override: Recommended
    // Default: Returns EOK and does nothing
    errno_t (*start)(void* _Nonnull _Locked self);

    // Invoked when a driver is made inactive. A driver subclass should
    // override this method and configure the hardware such that it is in an
    // idle and (if possible) powered-down state.
    // Override: Optional
    // Default: Does nothing
    void (*stop)(void* _Nonnull _Locked self);


    // Registers the driver in the I/O registry.
    // Override: Optional
    // Default: registers the driver with the I/O registry
    void (*doRegister)(void* _Nonnull self);

    // Deregisters the driver from the I/O registry.
    // Override: Optional
    // Default: deregisters the driver from the I/O registry
    void (*doDeregister)(void* _Nonnull self);

   
    // Invoked after the driver has been added as a child to the driver 'parent'.
    // Override: Optional; must call super
    // Default: Various management tasks
    void (*attachProvider)(void* _Nonnull self, DriverRef _Nonnull provider);

    // Invoked after the driver has been stopped, waited for stopped and right
    // before it is detached from 'parent'.
    // Override: Optional; must call super
    // Default: Various management tasks
    void (*detachProvider)(void* _Nonnull self, DriverRef _Nonnull provider);


    // Invoked by the open() function to inform the driver of another open. The
    // 'openCount' reflects the number of times the driver has been opened so
    // far. A count of 0 indicates that this is the first open. A driver
    // subclass can use this information to eg power up the hardware if necessary.
    // Override: Optional
    // Default: returns EOK
    errno_t (*onOpen)(void* _Nonnull _Locked self, int openCount, fd_flags_t flags);

    // Invoked by the close() function to close an open I/O handler. The
    // 'openCount' reflects the number of I/O handlers that are currently open
    // and this number does include the handler that should be closed. A driver
    // subclass can use this information to eg put the hardware to sleep if the
    // last open handler is closed. The 'openCount' for the last open handler is
    // 1.
    // Override: Optional
    // Default: does nothing
    void (*onClose)(void* _Nonnull _Locked self, int openCount);


    // Opens the driver for use. You need to call this function first before
    // calling any of the driver commands.
    // Override: Optional
    // Default: returns a handler instance
    errno_t (*open)(void* _Nonnull self, fd_flags_t flags);

    // Closes the driver.
    // Override: Optional
    // Default: Does nothing
    void (*close)(void* _Nonnull self);
);


// Attaches the driver 'self' to the provider 'provider' and then starts it and
// registers it with the I/O registry. The driver is active once this method
// returns and ready to receive calls to Driver_Open().
extern errno_t Driver_Launch(DriverRef _Nonnull self, DriverRef _Nullable provider);

// Terminates all clients of the driver 'self' and then 'self' itself.
extern void Driver_Terminate(DriverRef _Nonnull self);


// Opens the driver for use.
#define Driver_Open(__self, __flags) \
invoke_n(open, Driver, __self, __flags)

// Closes a driver.
#define Driver_Close(__self) \
invoke_0(close, Driver, __self)


// Returns true if the driver is currently in an opened state.
extern bool Driver_IsOpen(DriverRef _Nonnull self);


// Returns a reference to a read-only array of all the I/O categories the
// driver supports. This array is terminated by a IOCAT_END declaration.
#define Driver_GetCategories(__self) \
((const iocat_t*)((DriverRef)(__self)->cats)) 

// Returns true if the driver supports the given I/O category and false otherwise.
extern bool Driver_HasCategory(DriverRef _Nonnull self, iocat_t cat);

// Returns true if the driver supports any of the given I/O categories and false
// otherwise.
extern bool Driver_HasSomeCategories(DriverRef _Nonnull self, const iocat_t* _Nonnull cats);


// Returns the globally unique driver id. The id is assigned when the driver is
// published.
#define Driver_GetId(__self) \
((DriverRef)__self)->id


//
// Subclassers
//

// Creates a new driver instance.
// \param pClass the concrete driver class
// \param options options specifying various default behaviors
// \param cats the categories the driver conforms to. Note that the driver stores the provided reference. It does not copy the categories array. The array must be terminated with a IOCAT_END entry
extern errno_t Driver_Create(Class* _Nonnull pClass, unsigned options, const iocat_t* _Nonnull cats, DriverRef _Nullable * _Nonnull pOutSelf);


// Returns true if the driver is in active state; false otherwise
#define Driver_IsActive(__self) \
((((DriverRef)__self)->flags & kDriverFlag_IsActive) != 0)


// Returns the provider of the driver 'self'. A provider is another driver that
// provides services to 'self'. E.g. if 'self' is a SCSI disk driver, then the
// provider would be an IOSCSIDevice which provides functionality to issue SCSI
// commands to the bus. 
#define Driver_GetProvider(__self) \
((void*)((DriverRef)__self)->provider)

#define Driver_GetDevfsHandle(__self) \
((DriverRef)__self)->devfs_hnd


// Publish the receiver as a client driver to the driver catalog.
extern errno_t Driver_Publish(DriverRef _Nonnull self, const devfs_entry_t* _Nonnull en);

// Unpublishes the driver. Should be called from the onStop() override.
extern void Driver_Unpublish(DriverRef _Nonnull self);


#define Driver_AttachProvider(__self, __provider) \
invoke_n(attachProvider, Driver, __self, __provider)

#define Driver_DetachProvider(__self, __provider) \
invoke_n(detachProvider, Driver, __self, __provider)


// Starts a driver. This function is called by the Driver_Launch() function.
#define Driver_Start(__self) \
invoke_0(start, Driver, __self)

#define Driver_Stop(__self) \
invoke_0(stop, Driver, __self)


#define Driver_Register(__self) \
invoke_0(doRegister, Driver, __self)

#define Driver_Deregister(__self) \
invoke_0(doDeregister, Driver, __self)


extern void Driver_TerminateClients(DriverRef _Nonnull self);


#define Driver_OnOpen(__self, __openCount, __flags) \
invoke_n(onOpen, Driver, __self, __openCount, __flags)

#define Driver_OnClose(__self, __openCount) \
invoke_n(onClose, Driver, __self, __openCount)

#endif /* Driver_h */
