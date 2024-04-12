//
//  Runtime.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/16/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Object.h"
#include <klib/Memory.h>
#include <klib/Kalloc.h>
#ifndef __DISKIMAGE__
#include <klib/Log.h>

extern char _class;
extern char _eclass;
#endif


void _RegisterClass(ClassRef _Nonnull pClass)
{
    if ((pClass->flags & CLASSF_INITIALIZED) != 0) {
        return;
    }

    // Make sure that the super class is registered
    ClassRef pSuperClass = pClass->super;
    if (pSuperClass) {
        _RegisterClass(pSuperClass);
    }


    // Copy the super class vtable
    if (pSuperClass) {
        memcpy(pClass->vtable, pSuperClass->vtable, pSuperClass->methodCount * sizeof(Method));
    }


    // Override methods in the VTable with methods from our method list
    const struct MethodDecl* pCurMethod = pClass->methodList;
    while (pCurMethod->method) {
        Method* pSlot = (Method*)((char*)pClass->vtable + pCurMethod->offset);

        *pSlot = pCurMethod->method;
        pCurMethod++;
    }


    // Make sure that none if the VTable entries is NULL or a bogus pointer
    const int parentMethodCount = (pSuperClass) ? pSuperClass->methodCount : 0;
    for (int i = parentMethodCount; i < pClass->methodCount; i++) {
        if (pClass->vtable[i] == NULL) {
            fatal("RegisterClass: missing %s method at vtable index #%d\n", pClass->name, i);
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
    ClassRef pClass = (ClassRef)&_class;
    ClassRef pEndClass = (ClassRef)&_eclass;

    while (pClass < pEndClass) {
        _RegisterClass(pClass);
        pClass++;
    }
}

// Prints all registered classes
// Note that this function is not concurrency safe.
void PrintClasses(void)
{
    ClassRef pClass = (ClassRef)&_class;
    ClassRef pEndClass = (ClassRef)&_eclass;

    print("_class: %p, _eclass: %p\n", pClass, pEndClass);

    while (pClass < pEndClass) {
        if (pClass->super) {
            print("%s : %s\t\t", pClass->name, pClass->super->name);
        } else {
            print("%s\t\t\t\t", pClass->name);
        }
        print("mcount: %d\tisize: %zd\n", pClass->methodCount, pClass->instanceSize);

#if 0
        for (int i = 0; i < pClass->methodCount; i++) {
            print("%d: 0x%p\n", i, pClass->vtable[i]);
        }
#endif
        pClass++;
    }
}
#endif /* __DISKIMAGE__ */
