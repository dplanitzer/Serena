//
//  run_proc.c
//  status
//
//  Created by Dietmar Planitzer on 5/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef RUN_PROC_H
#define RUN_PROC_H 1

#include <stdint.h>
#include <ext/nanotime.h>
#include <ext/queue.h>
#include <serena/types.h>

#define MAX_PROC_NAME_LEN   127

#define RUN_PROC_RUNNING        0
#define RUN_PROC_SLEEPING       1
#define RUN_PROC_STOPPED        2
#define RUN_PROC_TERMINATING    3
#define RUN_PROC_TERMINATED     4


typedef struct run_proc {
    deque_node_t    pid_node;
    pid_t           pid;
    pid_t           pgrp;
    pid_t           ppid;
    pid_t           sid;
    uid_t           uid;
    size_t          vcpu_count;
    uint64_t        vm_size;
    nanotime_t      run_time;
    struct {
        unsigned int    alive:1;
        unsigned int    reserved:31;
    }               flags;
    int8_t          state;
    char            name[MAX_PROC_NAME_LEN + 1];
} run_proc_t;


typedef struct run_procs_info {
    nanotime_t  current_time;

    size_t      proc_count;
    size_t      run_proc_count;
    size_t      slp_proc_count;

    size_t      vcpu_count;
    
    size_t      cpu_count;
    
    uint64_t    phys_mem_size;
} run_procs_info_t;


extern void run_procs_sample(void);

extern run_proc_t* _Nullable run_proc_for_index(size_t idx);
extern size_t run_proc_count(void);

extern const char* _Nonnull run_proc_state_name(int state);

extern const run_procs_info_t* _Nonnull run_procs_info();

#endif /* RUN_PROC_H */