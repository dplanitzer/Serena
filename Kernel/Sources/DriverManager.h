//
//  DriverManager.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/1/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef DriverManager_h
#define DriverManager_h

#include "Foundation.h"
#include "Platform.h"

struct _DriverManager;
typedef struct _DriverManager* DriverManagerRef;

typedef void* DriverRef;

// The names of known drivers
extern const Character* const kGraphicsDriverName;
extern const Character* const kConsoleName;
extern const Character* const kEventsDriverName;
extern const Character* const kRealtimeClockName;


extern DriverManagerRef _Nonnull  gDriverManager;

extern ErrorCode DriverManager_Create(DriverManagerRef _Nullable * _Nonnull pOutManager);
extern void DriverManager_Destroy(DriverManagerRef _Nullable pManager);

// Does just as much auto configuration as is necessary to bring up the basic
// console services. Once this function returns the kernel printing subsystem
// can be initialized and used to print to the console.
extern ErrorCode DriverManager_AutoConfigureForConsole(DriverManagerRef _Nonnull pManager);

// Does a full auto configuration of all I/O devices.
extern ErrorCode DriverManager_AutoConfigure(DriverManagerRef _Nonnull pManager);

extern DriverRef DriverManager_GetDriverForName(DriverManagerRef _Nonnull pManager, const Character* pName);

extern ErrorCode DriverManager_GetExpansionBoardCount(DriverManagerRef _Nonnull pManager, Int* _Nonnull pOutCount);
extern ErrorCode DriverManager_GetExpansionBoardAtIndex(DriverManagerRef _Nonnull pManager, Int index, ExpansionBoard* _Nonnull pOutBoard);

#endif /* DriverManager_h */
