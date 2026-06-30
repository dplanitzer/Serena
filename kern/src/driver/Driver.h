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
// Drivers are organized in a driver hierarchy - also known as a driver tree.
// The root of the driver hierarchy is the 'platform controller'. This is a
// special kind of driver that knows how to detect all the various hardware
// components on a motherboard and how to create drivers for them. Many of the
// drivers that the platform controller creates are 'bus controllers'.
//
// A bus controller is a kind of driver that manages a physical or a virtual bus.
// It is responsible for detecting hardware that is connected to the bus and it
// kicks off the creation of bus client drivers that then in turn manage a
// piece of hardware that is connected to the bus.
//
// Every driver has a parent driver and it may have children drivers. The parent
// driver is usually the bus controller that manages the driver. However the
// parent driver may be an intermediate driver that sits between you and the bus
// controller. Your driver should use the services of the parent driver to issue
// commands to the bus. 
//
//
// -- The Driver Lifecycle --
//
// Every driver instance goes through a lifecycle. The various lifecycle stages
// are:
// - created: driver was just created
// - active: entered by calling Driver_Start()
// - stopping: entered by calling Driver_Stop() with a stop reason
// - stopped: entered by calling Driver_WaitForStopped()
// - ready for destroy: the driver may be destroyed once Driver_WaitForStopped()
//                      returns
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
// Once a driver has been started, driver handlers are created by calling
// Driver_Open(). The last Handler_Release() on a driver handler will trigger a
// call to Driver_Close().
//
// A driver may be voluntarily terminated by calling Driver_Stop(). If the
// driver is a bus controller (driver) then this stop will be automatically
// propagated to all the child drivers and the child drivers will be able to
// use I/O services of the bus driver to orderly shut down themselves.
//
// For example, a SCSI bus driver that stops will stop all of its bus clients by
// calling Driver_Stop() on them. This will allow the SCSI disk driver to use
// the I/O services of the SCSI bus driver to park the head of the disk.
// 
// The next step after calling Driver_Stop() is to invoke Driver_WaitForStopped().
// A driver may deploy asynchronous services internally and the shutdown of those
// services is triggered by the Driver_Stop() call. However it can take a while
// for those services to complete their shutdown. This is why it is necessary to
// wait by calling Driver_WaitForStopped() before you release the driver instance
// by calling Object_Release().
//
// A typical driver lifecycle looks like this:
//
// Driver_Create()
//   Driver_Start()
//     Driver_Open()
//       Handler_Read()
//       ...
//     Driver_Close()
//   Driver_Stop()
//   Driver_WaitForStopped()
// Object_Release()
//
// Note that an important purpose of I/O handlers is to enable the driver system
// to track whether a driver is in active use.
//
//
// -- Drivers, Concurrency and Exclusivity --
//
// I/O handler provides important preconditions for what is discussed below:
// *) All operations on an I/O handler are executed serially
// *) At most one operation can be active on an I/O handler at any given time
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
// caller passes in an I/O handler ensures that none of these operations can be
// executed before open() has completed and has returned a valid I/O handler.
//
// The fact that I/O handler guarantees that all active I/O operations on the
// handler have completed before it invokes close() on the driver ensures that
// close() can assume that no I/O operations can be active that are related to
// the handler that is passed to close(). Thus it is not necessary for close()
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
// -- The Driver Lifecycle and I/O Operations Locks --
//
// Every driver owns and manages two important locks.
//
// The first one is the lifecycle management lock. This lock is owned and
// managed by the Driver base class and subclasses do not need to worry about
// the details of this lock. This lock is used to protect the integrity of the
// driver state and it is used to ensure atomicity and exclusivity of the
// start(), stop(), open() and close() driver functions.
//
// The second lock is the I/O operations lock which is owned and managed by the
// Driver subclass. it is used to protect the integrity of the I/O related
// hardware and software state of the Driver subclass. It is also used to ensure
// atomicity and exclusivity of the read(), write() and ioctl() functions.
//
// It is the responsibility of a Driver subclass writer to implement the I/O
// operations lock and that it is acquired and released at the appropriate
// times.
//
//
// -- The Children of a Driver --
//
// A driver may create and manage child drivers. Child drivers are attached to
// their parent drivers and the parent driver maintains a strong reference to
// its child driver(s). This strong reference keeps a child driver alive as long
// as it remains attached to its parent. A child driver in turn receives an
// unowned reference to its immediate parent driver when it is attached to it.
// This reference remains valid until a driver is stopped and detached from its
// parent.
//
// The parent-child driver relationship can be used to properly represent
// relationships like a bus and the devices on a bus. The bus is represented
// by the parent driver and each device on the bus is represented by a child
// driver.
//
// Another use case for the parent-child driver relationship is that of a multi-
// function expansion board: an expansion board which features a sound chip and
// a CD-ROM driver can be represented by a parent driver that manages the overall
// card functionality plus a child driver for the sound chip and another child
// driver for the CD-ROM drive.
//
// A bus driver must explicitly configure the maximum number of children that it
// should manage. This number can be seen as the number of slots that are
// available on a physical bus. You configure this number by calling
// Driver_SetMaxChildCount() after you've created the bus driver instance and
// before you publish it to the driver catalog.
//
// A child driver may either receive a handler or a direct reference to its
// parent driver which it can then use to access the services that the parent
// driver provides. Both models are supported and the parent driver will stay
// alive as long as at least one handler is open or one child remains attached
// to it.
//
// A child driver is guaranteed that the reference to its parent driver will
// remain valid from the moment that its onStart() is invoked until the moment
// it returns from its onWaitForStopped() override.
//
//
// -- Parent - Child Driver Relationship and Starting/Stopping a Driver --
//
// A driver must be attached to a parent driver before it can be started to
// ensure that the driver will be able to access the services of its parent
// as soon as its onStart() method is called.
//
// A driver must be stopped before it can be detached from its parent driver to
// ensure that it continues to have access to the services that its parent driver
// provides until it has returned from its onWaitForStopped() override.
//
// The Driver_AttachChild() and Driver_DetachChild() methods implement and
// guarantee the semantics described in the previous paragraphs.
//
//
// -- What it Means for a Driver to be in Use --
//
// A driver is considered to be in use if:
// - at least one handler is open
// - at least one child is attached to it
// A driver that is in use may be stopped. However the driver isn't destroyed
// until after the last use of it is gone.
//
//
// -- Driver Categories --
//
// A driver conforms to a set of I/O categories. For many drivers it is sufficient
// to declare a single I/O category conformance. However a more complex driver
// that controls a piece of hardware that implements a number of different
// features will potentially declare one I/O category per feature. This is the
// reason why the Driver_Create() function takes an array of I/O categories. This
// array must be terminated with an IOCAT_END declaration. Note that the
// Driver_Create() function does not copy the I/O categories array. This array
// should be declared as a const static array. Use the IOCATS_DEF() macro to
// declare the I/O category array for your driver to get the correct declaration.
//
// Use the Driver_GetCategories() function to get a pointer to the read-only
// array of I/O categories the driver conforms to. This array is terminated by
// an IOCAT_END declaration.
//
// Use the Driver_HasCategory() function to check whether the driver conforms to
// a given I/O category.
//
// These functions can be used to easily and efficiently determine whether a
// driver controls hardware of a certain kind (eg whether it is a SCSI bus) and
// whether the hardware supports a certain kind of feature (eg whether a graphic
// card supports 2D and/or 3d hardware acceleration).
//
//
// -- Publishing a Driver and the Driver Catalog
//
// The driver manager maintains a driver catalog. The driver catalog stores an
// entry for every driver that should be openable. A driver is opened by calling
// open() with a path that point to a driver's catalog entry.
//
// The driver class provides two convenience APIs to publish a driver:
// * Publish: this should be used by a simple client driver that does not
//              implement a bus.
// * PublishBus: this should be used by a driver that implements a bus
//
// The PublishBus() function creates a directory to represent the bus and to
// provide a location where the bus clients can publish their respective driver
// entries. PublishBus() is additionally able to publish a 'self' entry in the
// bus directory. The 'self' entry represents the bus controller itself. This
// entry should be published if the bus controller wants to provide useful APIs
// to user space applications. Eg APIs to probe the bus, reset it or retrieve
// useful information about it.
//
// A driver should call the Publish() or PublishBus() function from its onStart()
// override. It is not necessary to override onStop() to call Unpublish() because
// the Driver base class takes care of un-publishing the driver automatically.
// Additionally, a onStart() override does not need to call Unpublish() if it
// encounters an error because Driver automatically unpublishes the driver if
// necessary if onStart() returns with an error.
//
// Unpublish is able to correctly unpublish a client driver and a bus driver.
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
    
    // Invoked as the result of calling Driver_Start(). A driver subclass should
    // override this method to reset the hardware, configure it such that
    // all components are in an idle state and to publish the driver to the
    // driver catalog.
    // Override: Recommended
    // Default: Returns EOK and does nothing
    errno_t (*onStart)(void* _Nonnull _Locked self);

    // Invoked as the result of calling Driver_Stop(). A driver subclass should
    // override this method and configure the hardware such that it is in an
    // idle and (if possible) powered-down state.
    // Override: Optional
    // Default: Does nothing
    void (*onStop)(void* _Nonnull _Locked self);


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


extern errno_t Driver_Launch(DriverRef _Nonnull client, DriverRef _Nullable provider);

extern void Driver_TerminateClients(DriverRef _Nonnull self);
extern void Driver_Terminate(DriverRef _Nonnull driver);


// Starts the driver. This function must be called after the driver has been
// attached to its parent driver and before an I/O command is invoked on the
// driver. It invokes the driver's onStart() override which finish the driver
// initialization and publishes the driver. This function automatically
// unpublishes the driver if its onStart() override returns with an error.
extern errno_t Driver_Start(DriverRef _Nonnull self);

// Stops the driver. 
extern void Driver_Stop(DriverRef _Nonnull self);

// Waits until the driver has finished its shutdown sequence. This specifically
// means that the caller is guaranteed that once this function returns that:
// *) no more I/O handlers are open referencing the driver
// *) no more asynchronous processes are active and referencing this driver
// The driver can be safely freed by calling Object_Release() once this function
// has returned.
extern void Driver_WaitForStopped(DriverRef _Nonnull self);


#define Driver_AttachProvider(__self, __provider) \
invoke_n(attachProvider, Driver, __self, __provider)

#define Driver_DetachProvider(__self, __provider) \
invoke_n(detachProvider, Driver, __self, __provider)

#define Driver_Register(__self) \
invoke_0(doRegister, Driver, __self)

#define Driver_Deregister(__self) \
invoke_0(doDeregister, Driver, __self)


// Opens the driver for use.
#define Driver_Open(__self, __flags) \
invoke_n(open, Driver, __self, __flags)

// Closes a driver handler.
#define Driver_Close(__self) \
invoke_0(close, Driver, __self)


// Returns true if there are open I/O handlers referencing this driver.
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


// Returns the immediate parent driver of the receiver. This is often the direct
// bus controller. However it may be an intermediate driver that sits between
// you and the bus controller.
#define Driver_GetProvider(__self) \
((void*)((DriverRef)__self)->provider)

#define Driver_GetDevfsHandle(__self) \
((DriverRef)__self)->devfs_hnd


//
// Subclassers
//

// Creates a new driver instance which will be attached to a virtual or physical
// bus.
// \param pClass the concrete driver class
// \param options options specifying various default behaviors
// \param cats the categories the driver conforms to. Note that the driver stores the provided reference. It does not copy the categories array. The array must be terminated with a IOCAT_END entry
extern errno_t Driver_Create(Class* _Nonnull pClass, unsigned options, const iocat_t* _Nonnull cats, DriverRef _Nullable * _Nonnull pOutSelf);


// Returns true if the driver is in active state; false otherwise
#define Driver_IsActive(__self) \
((((DriverRef)__self)->flags & kDriverFlag_IsActive) != 0)

// Returns the globally unique driver id. The id is assigned when the driver is
// published.
#define Driver_GetId(__self) \
((DriverRef)__self)->id


// Publish the receiver as a client driver to the driver catalog.
extern errno_t Driver_Publish(DriverRef _Nonnull self, const devfs_entry_t* _Nonnull en);

// Unpublishes the driver. Should be called from the onStop() override.
extern void Driver_Unpublish(DriverRef _Nonnull self);


#define Driver_OnStart(__self) \
invoke_0(onStart, Driver, __self)

#define Driver_OnStop(__self) \
invoke_0(onStop, Driver, __self)


#define Driver_OnOpen(__self, __openCount, __flags) \
invoke_n(onOpen, Driver, __self, __openCount, __flags)

#define Driver_OnClose(__self, __openCount) \
invoke_n(onClose, Driver, __self, __openCount)

#endif /* Driver_h */
