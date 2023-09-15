//
//  stdlib.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <syscall.h>
#include <__stddef.h>
#include "List.h"


typedef void (*AtExitFunc)(void);

typedef struct _AtExitEntry {
    SListNode           node;
    AtExitFunc _Nonnull func;
} AtExitEntry;


static SList    gAtExitQueue;


void __stdlibc_init(struct __process_arguments_t* _Nonnull argsp)
{
    SList_Init(&gAtExitQueue);
    __malloc_init();
}


// Not very efficient and that's fine. The expectation is that this will be used
// very rarely. So we rather keep the memory consumption very low for the majority
// that doesn't use this.
int atexit(void (*func)(void))
{
    AtExitEntry* p = (AtExitEntry*)malloc(sizeof(AtExitEntry));

    if (p == NULL) {
        return ENOMEM;
    }

    SListNode_Init(&p->node);
    p->func = func;
    SList_InsertBeforeFirst(&gAtExitQueue, &p->node);

    return 0;
}

_Noreturn exit(int exit_code)
{
    SList_ForEach(&gAtExitQueue, AtExitEntry, {
        pCurNode->func();
    });

    _Exit(exit_code);
}

_Noreturn _Exit(int exit_code)
{
    __syscall(SC_exit, exit_code);
}

int abs(int n)
{
    return __abs(n);
}

long labs(long n)
{
    return __abs(n);
}

long long llabs(long long n)
{
    return __abs(n);
}
