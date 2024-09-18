//
//  DriverManager.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/1/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef DriverManager_h
#define DriverManager_h

#include <klib/klib.h>
#include <hal/Platform.h>
#include "Driver.h"


// The names of known drivers
extern const char* const kGraphicsDriverName;
extern const char* const kConsoleName;
extern const char* const kEventsDriverName;
extern const char* const kRealtimeClockName;
extern const char* const kFloppyDrive0Name;


extern DriverManagerRef _Nonnull  gDriverManager;

extern errno_t DriverManager_Create(DriverManagerRef _Nullable * _Nonnull pOutSelf);
extern void DriverManager_Destroy(DriverManagerRef _Nullable self);

// Does just as much auto configuration as is necessary to bring up the basic
// console services. Once this function returns the kernel printing subsystem
// can be initialized and used to print to the console.
extern errno_t DriverManager_AutoConfigureForConsole(DriverManagerRef _Nonnull self);

// Does a full auto configuration of all I/O devices.
extern errno_t DriverManager_AutoConfigure(DriverManagerRef _Nonnull self);

extern DriverRef DriverManager_GetDriverForName(DriverManagerRef _Nonnull self, const char* name);

#endif /* DriverManager_h */
