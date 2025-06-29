//
//  kpi/vcpu.h
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_VCPU_H
#define _KPI_VCPU_H 1

// Replace the whole signal mask
#define SIGMASK_OP_REPLACE 0

// Enable the signals specified in the provided mask
#define SIGMASK_OP_ENABLE  1

// Disable the signals specified in the provided mask
#define SIGMASK_OP_DISABLE 2

#endif /* _KPI_VCPU_H */
