//
//  csw.h
//  Kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _CSW_H
#define _CSW_H 1

#include <stdnoreturn.h>
#include <kern/types.h>

//
// CPU Context Switcher API (used by the scheduler)
//

extern int csw_disable(void);
extern void csw_restore(int sps);

extern void csw_switch(void);
extern _Noreturn csw_switch_to_boot_vcpu(void);

#endif /* _CSW_H */
