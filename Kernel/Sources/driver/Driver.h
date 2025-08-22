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
#include <kpi/ioctl.h>
#include <kpi/stat.h>
#include <kpi/uid.h>
#include <sched/cnd.h>
#include <sched/mtx.h>
#include <Catalog.h>

struct drv_child;


// Driver instantiation option. Used by subclassers to request specific default
// behavior from the Driver class.
enum {
    kDriver_Exclusive = 1,  // At most one I/O channel can be open at any given time. Attempts to open more will generate a EBUSY error
    kDriver_IsBus = 2,      // This driver manages a hardware or virtual bus
};


// Driver stop reason. Used to indicate why a driver instance should be stopped.
enum {
    kDriverStop_Shutdown = 0,   // An orderly driver shutdown
    kDriverStop_HardwareLost,   // Driver should stop because the hardware was unexpectedly lost. Eg user disconnected the hardware without going through the user interface to do this in an orderly fashion
    kDriverStop_Abort,          // Driver detected a fatal error condition and is unable to continue
};


// Driver state. Used internally by the Driver class.
enum {
    kDriverState_Inactive = 0,
    kDriverState_Active,
    kDriverState_Stopping,
    kDriverState_Stopped
};


// Driver flags. Used internally by the Driver class.
enum {
    kDriverFlag_IsRoot = 1,             // This driver marks the root of a driver hierarchy
    kDriverFlag_IsAttached = 2,         // Driver is attached to a parent driver
    kDriverFlag_NoMoreChildren = 4,     // Driver has entered stopping state and no longer allows children to be attached
};


typedef struct DirEntry {
    CatalogId               dirId;
    const char* _Nonnull    name;
    uid_t                   uid;
    gid_t                   gid;
    mode_t                  perms;
} DirEntry;


typedef struct DriverEntry {
    CatalogId               dirId;
    const char* _Nonnull    name;
    uid_t                   uid;
    gid_t                   gid;
    mode_t                  perms;
    HandlerRef _Nullable    driver;
    intptr_t                arg;
} DriverEntry;


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
// kicks off the creation of bus client drivers that then manage the hardware
// that is connected to a bus.
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
// Once a driver has been started, driver channels may be created by calling
// Driver_Open() and a driver channel should be closed by calling
// IOChannel_Close() on the channel. IOChannel_Close() in turn invokes
// Driver_Close().
//
// A driver may be voluntarily terminated by calling Driver_Stop() with the.
// kDriverStop_Shutdown parameter. This indicates to the driver system that the
// driver wants to stop in a voluntarily and orderly fashion. If the driver is
// a bus controller (driver) then this stop will be automatically propagated to
// all the child drivers and the child drivers will be able to use I/O services
// of the bus driver to orderly shut down themselves.
//
// For example, a SCSI bus driver that stops will stop all of its bus clients by
// calling Driver_Stop() on them with the same reason that was passed to the
// SCSI bus driver. If the reason is kDriverStop_Shutdown then bus clients like
// a SCSI disk driver will be able to use the I/O services of the SCSI bus driver
// to park the head of the disk.
//
// If however the stop reason is kDriverStop_Abort or kDriverStop_HardwareLoss
// then the bus client drivers would not be allowed to use the SCSI bus driver
// I/O services since either the SCSI bus driver is in an undetermined state or
// it is no longer able to access the SCSI hardware.
// 
// The next step after calling Driver_Stop() is to invoke Driver_WaitForStopped().
// A driver may deploy asynchronous services internally and the shutdown of those
// services is triggered by the Driver_Stop() call. However it can take a while
// for those services to complete their shutdown. This is why it is necessary to
// wait by calling Driver_WaitForStopped() before you release the driver instance
// by calling Object_Release(). Note that the wait-for-stopped function also
// waits until after all open I/O channels on the driver have been closed.
//
// A typical driver lifecycle looks like this:
//
// Driver_Create()
//   Driver_Start()
//     Driver_Open()
//       IOChannel_Read()
//       ...
//     Driver_Close()
//   Driver_Stop()
//   Driver_WaitForStopped()
// Object_Release()
//
// Note that an important purpose of I/O channels is to enable the driver system
// to track whether a driver is in active use.
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
// atomicity and exclusivity of teh read(), write() and ioctl() functions.
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
// as it remains attached to its parent.
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
// A bus driver must explicitly configure the maximum number of children that it
// should manage. This number can be seen as the number of slots that are
// available on a physical bus. You configure this number by calling
// Driver_SetMaxChildCount() after you've created the bus driver instance and
// before you publish it to the driver catalog.
//
// A child driver may either receive a channel or a direct reference to its
// parent driver which it can then use to access the services that the parent
// driver provides. Both models are supported and the parent driver will stay
// alive as long as at least one channel is open or one child remains attached
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
// - at least one channel is open
// - at least one child is attached to it
// A driver that is in use may be stopped. However the driver isn't destroyed
// until after the last use of it is gone.
//
//
// -- Driver Relationships and Ownership --
//
// A parent driver owns its children and thus retains a child when it is added
// to the parent. A child driver should not retain its parent. It should only
// maintain a weak reference to its parent driver.
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
open_class(Driver, Handler,
    mtx_t                       mtx;    // lifecycle management lock
    cnd_t                       cnd;
    const iocat_t* _Nonnull     cats;   // categories the driver conforms to.
    did_t                       id;     // unique id assigned at publish time
    CatalogId                   ownedBusDirId;  // bus directory that this driver has published
    
    DriverRef _Nullable         parent; // weak ref to the parent driver; constant over the lifetime of the driver
    struct drv_child* _Nullable child;
    mtx_t                       childMtx;

    uint16_t                    options;
    uint8_t                     flags;
    int8_t                      state;      //XXX should be atomic_int
    int                         openCount;
    int16_t                     maxChildCount;
    int8_t                      stopReason;
);
open_class_funcs(Driver, Handler,
    
    // Invoked as the result of calling Driver_Start(). A driver subclass should
    // override this method to reset the hardware, configure it such that
    // all components are in an idle state and to publish the driver to the
    // driver catalog.
    // Override: Recommended
    // Default Behavior: Returns EOK and does nothing
    errno_t (*onStart)(void* _Nonnull _Locked self);

    // Invoked as the result of calling Driver_Stop(). A driver subclass should
    // override this method and configure the hardware such that it is in an
    // idle and (if possible) powered-down state.
    // Override: Optional
    // Default Behavior: Does nothing
    void (*onStop)(void* _Nonnull _Locked self);

    // Invoked as the result of calling Driver_WaitForStopped(). A driver
    // subclass that deploys asynchronous processes as part of its implementation
    // should override this method and wait for those processes to be terminated.
    // The shutdown of those processes should be triggered from an onStop()
    // override.
    // Override: Optional
    // Default Behavior: Does nothing
    void (*onWaitForStopped)(void* _Nonnull self);

   
    // Invoked after the driver has been added as a child to the driver 'parent'.
    // Override: Optional; must call super
    // Default Behavior: Various management tasks
    void (*onAttached)(void* _Nonnull self, DriverRef _Nonnull parent);

    // Invoked after the driver has been stopped, waited for stopped and right
    // before it is detached from 'parent'.
    // Override: Optional; must call super
    // Default Behavior: Various management tasks
    void (*onDetaching)(void* _Nonnull self, DriverRef _Nonnull parent);


    // Invoked by the open() function to create the driver channel that should
    // be returned to the caller. The 'openCount' reflects the number of I/O
    // channels that are currently open for this driver. Note that this count
    // does not yet include the channel that should be created. A count of 0
    // indicates that this is the first channel that should be opened. A driver
    // subclass can use this information to eg power up the hardware if
    // necessary.
    // Override: Optional
    // Default Behavior: returns a DriverChannel instance
    errno_t (*onOpen)(void* _Nonnull _Locked self, int openCount, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel);

    // Invoked by the close() function to close an open I/O channel. The
    // 'openCount' reflects the number of I/O channels that are currently open
    // and this number does include the channel that should be closed. A driver
    // subclass can use this information to eg put the hardware to sleep if the
    // last open channel is closed. The 'openCount' for the last open channel is
    // 1. Note that this function should not release the I/O channel. This is
    // taken care off by the kernel.
    // Override: Optional
    // Default Behavior: does nothing
    void (*onClose)(void* _Nonnull _Locked self, IOChannelRef _Nonnull ioc, int openCount);


    // Publish the driver. Should be called from the onStart() override.
    errno_t (*publish)(void* _Nonnull self, const DriverEntry* _Nonnull de);

    // Unpublishes the driver. Should be called from the onStop() override.
    void (*unpublish)(void* _Nonnull self);
);


// Starts the driver. This function must be called after the driver has been
// attached to its parent driver and before an I/O command is invoked on the
// driver. It causes the driver to finish initialization and to publish its
// catalog entry to the driver catalog.
extern errno_t Driver_Start(DriverRef _Nonnull self);

// Stops the driver. 'reason' specifies the reason why the driver should be
// stopped. This method first stops all children of the receiver and it then
// changes the state to stopping and it finally unpublishes the driver and its
// bus directory (if it published one) and it then invokes the onStop() lifecycle
// method. A driver subclass should override onStop() and program the hardware
// in such a way that it is effectively disabled and no longer active. A child
// driver which is told to stop by its controlling parent (bus) driver should
// check the reason for the stop: The reason 'shutdown' indicates that the stop
// is orderly and was voluntarily triggered. The parent driver is still active
// and is still willing to accept I/O requests. The child driver may issue I/O
// requests to its parent to eg park the head of its harddisk (assuming the
// child driver manages a hard disk). A reason of 'abort' or 'hardware-lost'
// indicates that the stop is not orderly and that the parent/bus and the child
// driver are no longer able to access the hardware. A driver should simply free
// resources and not attempt to access hardware or issues I/O requests to its
// parent driver in this case. 
extern void Driver_Stop(DriverRef _Nonnull self, int reason);

// Waits until the driver has finished its shutdown sequence. This specifically
// means that the caller is guaranteed that once this function returns that:
// *) no more I/O channels are open referencing the driver
// *) no more asynchronous processes are active and referencing this driver
// The driver can be safely freed by calling Object_Release() once this function
// has returned.
extern void Driver_WaitForStopped(DriverRef _Nonnull self);


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


// Returns true if there are open I/O channels referencing this driver.
extern bool Driver_HasOpenChannels(DriverRef _Nonnull self);


// Returns a reference to a read-only array of all the I/O categories the
// driver supports. This array is terminated by a IOCAT_END declaration.
#define Driver_GetCategories(__self) \
((DriverRef)(__self)->cats) 

// Returns true if the driver supports the given I/O category and false otherwise.
extern bool Driver_HasCategory(DriverRef _Nonnull self, iocat_t cat);

// Returns true if the driver supports any of the given I/O categories and false
// otherwise.
extern bool Driver_HasSomeCategories(DriverRef _Nonnull self, const iocat_t* _Nonnull cats);


// Returns the bus controller that directly controls the receiver
extern DriverRef _Nullable Driver_GetBusController(DriverRef _Nonnull self);

#define Driver_GetBusControllerAs(__self, __class) \
((__class##Ref)Driver_GetBusController((DriverRef)__self))


// Returns the immediate parent driver of the receiver. This is often the direct
// bus controller. However it may be an intermediate driver that sits between
// you and the bus controller.
#define Driver_GetParent(__self) \
((DriverRef)__self)->parent

#define Driver_GetParentAs(__self, __class) \
((__class##Ref)(((DriverRef)__self)->parent))


//
// Subclassers
//

// Creates a new driver instance which will be attached to a virtual or physical
// bus.
// \param pClass the concrete driver class
// \param options options specifying various default behaviors
// \param cats the categories the driver conforms to. Note that the driver stores the provided reference. It does not copy the categories array. The array must be terminated with a IOCAT_END entry
extern errno_t Driver_Create(Class* _Nonnull pClass, unsigned options, const iocat_t* _Nonnull cats, DriverRef _Nullable * _Nonnull pOutSelf);

// Creates a new driver instance which represents the root of a driver hierarchy.
// \param pClass the concrete driver class
// \param options options specifying various default behaviors
// \param cats the categories the driver conforms to. Note that the driver stores the provided reference. It does not copy the categories array. The array must be terminated with a IOCAT_END entry
extern errno_t Driver_CreateRoot(Class* _Nonnull pClass, unsigned options, const iocat_t* _Nonnull cats, DriverRef _Nullable * _Nonnull pOutSelf);


// Returns true if the driver is in active state; false otherwise
#define Driver_IsActive(__self) \
(((DriverRef)__self)->state == kDriverState_Active ? true : false)

// Returns the globally unique driver id. The id is assigned when the driver is
// published.
#define Driver_GetId(__self) \
((DriverRef)__self)->id

// Returns the bus directory of the bus controller that manages the receiver.
// This is the directory inside of which the receiver should place its own
// driver catalog entries (driver and bus entry).
extern CatalogId Driver_GetBusDirectory(DriverRef _Nonnull self);

// Returns the bus directory that was created and is managed by the receiver.
#define Driver_GetPublishedBusDirectory(__self) \
((DriverRef)__self)->ownedBusDirId


// Publish the driver. Should be called from the onStart() override.
#define Driver_Publish(__self, __de) \
invoke_n(publish, Driver, __self, __de)

// Unpublishes the driver. Should be called from the onStop() override.
#define Driver_Unpublish(__self) \
invoke_0(unpublish, Driver, __self)

// Publishes a bus directory
extern errno_t Driver_PublishBusDirectory(DriverRef _Nonnull self, const DirEntry* _Nonnull de);

// Unpublishes the bus directory that the receiver has previously published
extern void Driver_UnpublishBusDirectory(DriverRef _Nonnull self);


#define Driver_OnStart(__self) \
invoke_0(onStart, Driver, __self)

#define Driver_OnStop(__self) \
invoke_0(onStop, Driver, __self)

#define Driver_OnWaitForStopped(__self) \
invoke_0(onWaitForStopped, Driver, __self)


#define Driver_OnAttached(__self, __parent) \
invoke_n(onAttached, Driver, __self, __parent)

#define Driver_OnDetaching(__self, __parent) \
invoke_n(onDetaching, Driver, __self, __parent)


// Creates an I/O channel that connects the driver to a user space application
// or a kernel space service
#define Driver_OnOpen(__self, __openCount, __mode, __arg, __pOutChannel) \
invoke_n(onOpen, Driver, __self, __openCount, __mode, __arg, __pOutChannel)

// Closes the given I/O channel
#define Driver_OnClose(__self, __ioc, __openCount) \
invoke_n(onClose, Driver, __self, __ioc, __openCount)


// Specifies the number of children that a driver is able to manage. This number
// corresponds to the number of slots available on the bus that the driver
// manages. The number is 0 by default. A driver that manages a physical or
// virtual bus should call this method with a suitable number before it calls
// Driver_Publish() on itself. Returns EINVAL if 'count' is bigger than the
// system imposed upper limit. Returns EBUSY if this function is called while
// the driver has children attached to it. Resizing a bus is only allowed
// while the bus driver has no children attached to it.
extern errno_t Driver_SetMaxChildCount(DriverRef _Nonnull self, size_t count);

// Returns the max child count.
extern size_t Driver_GetMaxChildCount(DriverRef _Nonnull self);

// Returns the number of child drivers that are currently attached to the
// receiver.
extern size_t Driver_GetChildCount(DriverRef _Nonnull self);

// Returns true if the receiver has at least one child attached to it; false
// otherwise.
extern bool Driver_HasChildren(DriverRef _Nonnull self);


// Returns the lowest numbered slot id that is unoccupied. Returns -1 if no slot
// is available.
extern ssize_t Driver_GetFirstAvailableSlotId(DriverRef _Nonnull self);


// Returns an unowned reference to the child driver with bus slot id 'slotId'.
// NULL is returned if the slot is empty. Note that you must retain the returned
// driver reference explicitly if you plan to continue to use it after it has
// been removed from the driver.
extern DriverRef _Nullable Driver_GetChildAt(DriverRef _Nonnull self, size_t slotId);

// Like as Driver_GetChildAt() but returns a strong reference to the requested
// child driver.
extern DriverRef _Nullable Driver_CopyChildAt(DriverRef _Nonnull self, size_t slotId);


// Attaches the driver 'child' as a child driver to the receiver while taking
// ownership of the provided reference. The driver is attached at the bus slot
// 'slotId'. EBUSY is returned if this slot already has a driver attached to it.
extern errno_t Driver_AttachChild(DriverRef _Nonnull self, DriverRef _Nonnull child, size_t slotId);

// Like Driver_AttachChild() but additionally starts the driver 'child'. Driver
// 'child' is detached if the start operation fails.
extern errno_t Driver_AttachStartChild(DriverRef _Nonnull self, DriverRef _Nonnull child, size_t slotId);

// Stops the child at the bus slot id 'slotId' with stop reason 'reason' and
// waits for the stop to complete. Does nothing if the slot contains no driver.
// The child is stopped, waited for stopped and then released and removed from
// the child list.
extern void Driver_DetachChild(DriverRef _Nonnull self, int stopReason, size_t slotId);


// Returns the data associated with the bus slot id 'slotId'. 0 is returned if
// no data has ever been set on the bus slot.
extern intptr_t Driver_GetChildDataAt(DriverRef _Nonnull self, size_t slotId);

// Replaces the data currently associated with bus slot id 'slotId' with the
// provided data. The data is stored by value. Also note that the data is
// managed independently from the child driver. Eg removing the child driver
// for a slot does not automatically remove the associated data. This function
// returns the data that was previously associated with the slot.
extern intptr_t Driver_SetChildDataAt(DriverRef _Nonnull self, size_t slotId, intptr_t data);

#endif /* Driver_h */
