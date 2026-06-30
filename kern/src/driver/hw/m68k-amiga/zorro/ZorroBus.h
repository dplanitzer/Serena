//
//  ZorroBus.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef ZorroBus_h
#define ZorroBus_h

#include <driver/IODriver.h>
#include <machine/amiga/zorro.h>


// The ZorroBus is responsible for auto-configuring and managing the
// Zorro bus in your Amiga computer. There's exactly one instance of this driver
// in the system.
//
// The ZorroBus enumerates all Zorro boards when the machine is powered
// up or reset. It assigns each board to an available address range and it
// creates an instance of the ZorroDevice class for each board. The ZorroDevice
// for a board then figures out which board specific driver should be used to
// control that board and it instantiates and starts this board driver.
//
// The board driver receives the ZorroDevice instance as its parent and it uses
// it to access the Zorro configuration information for the board.
//
// This design allows a board driver to derive from an arbitrary Driver subclass
// instead of forcing it to derive from a specific class, and thus it provides a
// lot more flexibility.
//
final_class(ZorroBus, IODriver);

extern errno_t ZorroBus_Create(ZorroBusRef _Nullable * _Nonnull pOutSelf);

#endif /* ZorroBus_h */
