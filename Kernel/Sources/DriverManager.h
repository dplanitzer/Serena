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
#include "Platform.h"

struct _DriverManager;
typedef struct _DriverManager* DriverManagerRef;

typedef void* DriverRef;

// The names of known drivers
extern const char* const kGraphicsDriverName;
extern const char* const kConsoleName;
extern const char* const kEventsDriverName;
extern const char* const kRealtimeClockName;


extern DriverManagerRef _Nonnull  gDriverManager;

extern errno_t DriverManager_Create(DriverManagerRef _Nullable * _Nonnull pOutManager);
extern void DriverManager_Destroy(DriverManagerRef _Nullable pManager);

// Does just as much auto configuration as is necessary to bring up the basic
// console services. Once this function returns the kernel printing subsystem
// can be initialized and used to print to the console.
extern errno_t DriverManager_AutoConfigureForConsole(DriverManagerRef _Nonnull pManager);

// Does a full auto configuration of all I/O devices.
extern errno_t DriverManager_AutoConfigure(DriverManagerRef _Nonnull pManager);

extern DriverRef DriverManager_GetDriverForName(DriverManagerRef _Nonnull pManager, const char* pName);

extern int DriverManager_GetExpansionBoardCount(DriverManagerRef _Nonnull pManager);
extern ExpansionBoard DriverManager_GetExpansionBoardAtIndex(DriverManagerRef _Nonnull pManager, int index);

#endif /* DriverManager_h */
