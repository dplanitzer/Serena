//
//  Class.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/16/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Object.h"
#include <klib/Memory.h>
#include <klib/Kalloc.h>
#ifndef __DISKIMAGE__
#include <log/Log.h>

extern char _class;
extern char _eclass;
#endif


void _RegisterClass(Class* _Nonnull pClass)
{
    if ((pClass->flags & CLASSF_INITIALIZED) != 0) {
        return;
    }

    // Make sure that the super class is registered
    Class* pSuperClass = pClass->super;
    if (pSuperClass) {
        _RegisterClass(pSuperClass);
    }


    // Copy the super class vtable
    if (pSuperClass) {
        memcpy(pClass->vtable, pSuperClass->vtable, pSuperClass->methodCount * sizeof(MethodImpl));
    }


    // Initialize our vtable with the methods from our method list
    const struct MethodDecl* pCurMethod = pClass->methodList;
    while (pCurMethod->method) {
        MethodImpl* pSlot = (MethodImpl*)((char*)pClass->vtable + pCurMethod->offset);

        *pSlot = pCurMethod->method;
        pCurMethod++;
    }


    // Make sure that none if the VTable entries is NULL or a bogus pointer
    const size_t parentMethodCount = (pSuperClass) ? pSuperClass->methodCount : 0;
    for (size_t i = parentMethodCount; i < pClass->methodCount; i++) {
        if (pClass->vtable[i] == NULL) {
            fatal("RegisterClass: missing %s method at vtable index #%zu\n", pClass->name, i);
            // NOT REACHED
        }
    }

    pClass->flags |= CLASSF_INITIALIZED;
}

#ifndef __DISKIMAGE__
// Scans the "__class" data section bounded by the '_class' and '_eclass' linker
// symbols for class records and:
// - builds the vtable for each class
// - validates the vtable
// Must be called after the DATA and BSS segments have been established and before
// and code is invoked that might use objects.
// Note that this function is not concurrency safe.
void RegisterClasses(void)
{
    Class* pClass = (Class*)&_class;
    Class* pEndClass = (Class*)&_eclass;

    while (pClass < pEndClass) {
        _RegisterClass(pClass);
        pClass++;
    }
}

#if 0
// Prints all registered classes
// Note that this function is not concurrency safe.
void PrintClasses(void)
{
    Class* pClass = (Class*)&_class;
    Class* pEndClass = (Class*)&_eclass;

    printf("_class: %p, _eclass: %p\n", pClass, pEndClass);

    while (pClass < pEndClass) {
        if (pClass->super) {
            printf("%s : %s\t\t", pClass->name, pClass->super->name);
        } else {
            printf("%s\t\t\t\t", pClass->name);
        }
        printf("mcount: %d\tisize: %zd\n", pClass->methodCount, pClass->instanceSize);

#if 0
        for (size_t i = 0; i < pClass->methodCount; i++) {
            printf("%zu: 0x%p\n", i, pClass->vtable[i]);
        }
#endif
        pClass++;
    }
}
#endif
#endif /* __DISKIMAGE__ */
