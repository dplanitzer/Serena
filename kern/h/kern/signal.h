//
//  kern/signal.h
//  libc
//
//  Created by Dietmar Planitzer on 5/03/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_SIGNAL_H
#define _KERN_SIGNAL_H 1

#include <kpi/signal.h>
#include <kpi/types.h>

typedef struct sig_sndr {
    pid_t   pid;
    uid_t   uid;
} sig_sndr_t;

typedef struct sig_rcvr {
    int         target;
    pid_t       id;     // pid, pgrp, sid
    vcpuid_t    vid;    // vcpu, vcpu group
} sig_rcvr_t;


#define sig_make_sender(__pid, __uid) \
(sig_sndr_t){__pid, __uid}

#define sig_make_receiver(__target, __pid, __vid) \
(sig_rcvr_t){__target, __pid, __vid}

#endif /* _KERN_SIGNAL_H */
