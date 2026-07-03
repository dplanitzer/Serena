//
//  IODriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef IODriver_h
#define IODriver_h

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
#define kIODriver_Exclusive       1   // At most one I/O handler can be open at any given time. Attempts to open more will generate a EBUSY error


typedef errno_t (*IOHandlerFunc)(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nullable pOutHandler);

// The max length of a devfs name entry (includes the trailing '\0' character).
// The system will automatically make the name unique if necessary. It does this
// by inserting a decimal number into the provided string. Place a '$' character
// inside teh string to specify where the number should be inserted. The number
// is appended to the string if no '$' is inside the string. The '$' is removed
// from the string and the provided string is used as provided if it is already
// unique.
#define kIODFSMaxName   48


typedef struct IODFSInfo {
    char                    name[kIODFSMaxName];
    IOHandlerFunc _Nonnull  func;
    uid_t                   uid;
    gid_t                   gid;
    fs_perms_t              perms;
} IODFSInfo;


// A driver object manages a device. A device is a piece of hardware while a
// driver is the software that manages the hardware.
//
// A driver may manage some piece of hardware directly or it may employ the help
// of another driver to do this. This other driver is called a provider and the
// driver that is using the provider is called a client driver.
//
// The IODriver base class implements a lifecycle mechanism. The lifecycle of a
// driver looks like this:
//
// - Driver_Create()            driver is created
// - Driver_Launch()            driver is started
//     - Driver_Start()         driver start() override is invoked
//     - Driver_OnLaunched()    driver onLaunched() override is invoked
// - Driver_Open()              user app, kernel or another driver is opening the driver for use
// - Commands                   driver user is issuing I/O commands to the driver
// - Driver_Close()             driver user is no longer interested in using the driver
// - Driver_Terminate()         driver is told to shut down and stop accessing its provider/hardware
//     - Driver_OnTerminating() driver onTerminating() override is invoked
//     - Driver_Stop()          driver stop() override is invoked
// - Driver_AwaitTermination()  driver user is potentially waiting for driver termination to complete
//
// Every driver implements a set of I/O commands that require that the driver either
// uses the services of its provider or that it programs the hardware that it manages.
// 
// A driver is allowed to access and program its hardware from the beginning of
// its start() override until it either returns true from its stop() override
// or until it calls IODriver_TerminationCompleted().
// 
// Once a driver has terminated and before it has launched, it may not access nor
// program its hardware. A driver client that call IODriver_Terminate() on a driver
// and that needs to know when the driver has stopped accessing and programming
// its hardware must call IODriver_AwaitTermination() after IODriver_Terminate()
// has returned. Then and only then is it safe to access the hardware in question.
//
// A driver must be launched before it can be opened and a driver can not be
// opened anymore once IODriver_Terminate() has been called on it. However, you
// can still call IODriver_Close() even after a driver has been terminated.
//
open_class(IODriver, Object,
    deque_node_t                ioreg_qe;   // dequeue is owned and managed by IORegistry
    IODriverRef _Nullable       provider;   // strong ref to the provider driver; immutable between start() and stop()
    did_t                       id;         // globally unique driver id (> 0)

    mtx_t                       mtx;        // lifecycle management lock
    const iocat_t* _Nonnull     cats;       // categories the driver conforms to.
    devfs_hnd_t                 devfs_hnd;

    int                         open_cnt;
    uint16_t                    options;
    uint8_t                     flags;
    int8_t                      state;
);
open_class_funcs(IODriver, Object,
    
    // Invoked after the driver has been added as a child to the driver 'parent'.
    // Override: Optional; must call super
    // Default: Various management tasks
    void (*attachProvider)(void* _Nonnull self, IODriverRef _Nonnull provider);

    // Invoked after the driver has been stopped, waited for stopped and right
    // before it is detached from 'parent'.
    // Override: Optional; must call super
    // Default: Various management tasks
    void (*detachProvider)(void* _Nonnull self, IODriverRef _Nonnull provider);


    // Invoked when a driver is made active. Subclasses should override this
    // method, reset the hardware and/or establish the basic hardware
    // configuration and allocate resources that will be needed whether a client
    // is actually using the driver or not.
    // Override: Recommended
    // Default: Returns EOK and does nothing
    errno_t (*start)(void* _Nonnull self);

    // Invoked when a driver is terminated. Subclassers should stop accepting
    // new I/O commands and either let the currently active command finish or
    // cancel it. This function should return true if it has stopped the driver
    // and the driver is no longer accessing its provider or hardware. It should
    // return false if the stopping process must be done asynchronously. It should
    // call IODriver_TerminationCompleted() once the stop process has finished.
    // Override: Optional
    // Default: Does nothing and returns true
    bool (*stop)(void* _Nonnull self);


    // Attaches the driver 'self' to the provider 'provider' and then starts it and
    // registers it with the I/O registry. The driver is active once this method
    // returns and ready to receive calls to Driver_Open().
    errno_t (*launch)(void* _Nonnull self, IODriverRef _Nullable provider);

    // Invoked at the end of IODriver_Launch(). Subclassers may override this
    // to kick off more initialization work/commands after the driver has been
    // started and registered. E.g. a bus driver could scan the bus for devices,
    // create drivers and launch them as needed. The driver is no longer locked
    // when this method is called.
    // Override: Optional
    // Default: Does nothing
    void (*onLaunched)(void* _Nonnull self);


    // Terminates all clients of the driver 'self' and then 'self' itself. Note that
    // the termination may run asynchronously. Thus this function may return before
    // the termination has completed. Use the IODriver_AwaitTermination() function
    // to await the completion of the termination if this matters to you.
    void (*terminate)(void* _Nonnull self);

    // Invoked at the beginning of IODriver_Terminate(). Subclassers may
    // override this to e.g. cancel queued commands.
    // Override: Optional
    // Default: Does nothing
    void (*onTerminating)(void* _Nonnull self);


    // Invoked to get a description of the devfs entry that should be created
    // for the driver. Return ENOTSUP if no devfs entry should be created for
    // the driver. A devfs entry is created after the driver has been registered
    // with the I/O registry. The returned driver name may contain a '$' character.
    // The '$' character is used as an indication where the system should insert
    // a number to make the entry unique if needed. If the name has no '$'
    // character then the number to unique the entry is appended to the end of
    // the name.
    // Override: Optional
    // Default: Returns ENOTSUP and no devfs entry is created
    errno_t (*getDFSInfo)(void* _Nonnull self, IODFSInfo* _Nonnull info);


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


// Launch a driver.
#define IODriver_Launch(__self, __provider) \
invoke_n(launch, IODriver, __self, __provider)

// Terminate a driver.
#define IODriver_Terminate(__self) \
invoke_0(terminate, IODriver, __self)


// Awaits the termination of the driver by blocing the caller until teh driver
// has reached terminated state.
extern void IODriver_AwaitTermination(IODriverRef _Nonnull self);


// Opens the driver for use.
#define IODriver_Open(__self, __flags) \
invoke_n(open, IODriver, __self, __flags)

// Closes a driver.
#define IODriver_Close(__self) \
invoke_0(close, IODriver, __self)


// Returns true if the driver is currently in an opened state.
extern bool IODriver_IsOpen(IODriverRef _Nonnull self);


// Returns a reference to a read-only array of all the I/O categories the
// driver supports. This array is terminated by a IOCAT_END declaration.
#define IODriver_GetCategories(__self) \
((const iocat_t*)((IODriverRef)(__self)->cats)) 

// Returns true if the driver supports the given I/O category and false otherwise.
extern bool IODriver_HasCategory(IODriverRef _Nonnull self, iocat_t cat);

// Returns true if the driver supports any of the given I/O categories and false
// otherwise.
extern bool IODriver_HasSomeCategories(IODriverRef _Nonnull self, const iocat_t* _Nonnull cats);


// Returns the globally unique driver id. The id is assigned when the driver is
// published.
#define IODriver_GetId(__self) \
((IODriverRef)__self)->id


//
// Subclassers
//

// Creates a new driver instance.
// \param pClass the concrete driver class
// \param options options specifying various default behaviors
// \param cats the categories the driver conforms to. Note that the driver stores the provided reference. It does not copy the categories array. The array must be terminated with a IOCAT_END entry
extern errno_t IODriver_Create(Class* _Nonnull pClass, unsigned options, const iocat_t* _Nonnull cats, IODriverRef _Nullable * _Nonnull pOutSelf);

// Notifies the framework that the driver 'self' has completed termination. This
// call will wake up a blocked IODriver_AwaitTermination() call.
extern void IODriver_TerminationCompleted(IODriverRef _Nonnull self);


// Returns the provider of the driver 'self'. A provider is another driver that
// provides services to 'self'. E.g. if 'self' is a SCSI disk driver, then the
// provider would be an IOSCSIDevice which provides functionality to issue SCSI
// commands to the bus. 
#define IODriver_GetProvider(__self) \
((void*)((IODriverRef)__self)->provider)

#define IODriver_AttachProvider(__self, __provider) \
invoke_n(attachProvider, IODriver, __self, __provider)

#define IODriver_DetachProvider(__self, __provider) \
invoke_n(detachProvider, IODriver, __self, __provider)


// Starts a driver. This function is called by the IODriver_Launch() function.
#define IODriver_Start(__self) \
invoke_0(start, IODriver, __self)

#define IODriver_Stop(__self) \
invoke_0(stop, IODriver, __self)


#define IODriver_OnLaunched(__self) \
invoke_0(onLaunched, IODriver, __self)

#define IODriver_OnTerminating(__self) \
invoke_0(onTerminating, IODriver, __self)


#define IODriver_GetDFSInfo(__self, __info) \
invoke_n(getDFSInfo, IODriver, __self, __info)

#define IODriver_GetDFSHandle(__self) \
((IODriverRef)__self)->devfs_hnd


#define IODriver_OnOpen(__self, __openCount, __flags) \
invoke_n(onOpen, IODriver, __self, __openCount, __flags)

#define IODriver_OnClose(__self, __openCount) \
invoke_n(onClose, IODriver, __self, __openCount)

#endif /* IODriver_h */
