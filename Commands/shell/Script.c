//
//  Script.c
//  sh
//
//  Created by Dietmar Planitzer on 1/2/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Script.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Atom
////////////////////////////////////////////////////////////////////////////////

errno_t Atom_Create(StackAllocatorRef _Nonnull pAllocator, AtomType type, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Atom* self = NULL;
    
    try_null(self, StackAllocator_ClearAlloc(pAllocator, sizeof(Atom)), ENOMEM);
    self->type = type;
    self->hasLeadingWhitespace = hasLeadingWhitespace;

catch:
    *pOutSelf = self;
    return err;
}

errno_t Atom_CreateWithCharacter(StackAllocatorRef _Nonnull pAllocator, char ch, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Atom* self = NULL;

    try(Atom_Create(pAllocator, kAtom_Character, hasLeadingWhitespace, &self));
    self->u.character = ch;   

catch:
    *pOutSelf = self;
    return err;
}

errno_t Atom_CreateWithString(StackAllocatorRef _Nonnull pAllocator, AtomType type, const char* _Nonnull str, size_t len, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Atom* self = NULL;
    
    try_null(self, StackAllocator_ClearAlloc(pAllocator, sizeof(Atom) + len + 1), ENOMEM);
    self->type = type;
    self->hasLeadingWhitespace = hasLeadingWhitespace;
    self->u.string.chars = ((char*)self) + sizeof(Atom);
    memcpy(self->u.string.chars, str, len);
    self->u.string.chars[len] = '\0';
    self->u.string.length = len;

catch:
    *pOutSelf = self;
    return err;
}

errno_t Atom_CreateWithPExpression(StackAllocatorRef _Nonnull pAllocator, PExpression* _Nonnull expr, Atom* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Atom* self = NULL;

    try(Atom_Create(pAllocator, kAtom_PExpression, true, &self));
    self->u.expr = expr;

catch:
    *pOutSelf = self;
    return err;
}

#ifdef SCRIPT_PRINTING
void Atom_Print(Atom* _Nonnull self)
{
    if (self->hasLeadingWhitespace) {
        putchar(' ');
    }

    switch (self->type) {
        case kAtom_Character:
            putchar(self->u.character);
            break;
            
        case kAtom_UnquotedString:
            fputs(self->u.string.chars, stdout);
            break;

        case kAtom_SingleQuotedString:
            printf("'%s'", self->u.string.chars);
            break;

        case kAtom_DoubleQuotedString:
            printf("\"%s\"", self->u.string.chars);
            break;

        case kAtom_EscapedCharacter:
            printf("\\%s", self->u.string.chars);
            break;

        case kAtom_VariableReference:
            printf("$%s", self->u.string.chars);
            break;

        case kAtom_PExpression:
            putchar('(');
            PExpression_Print(self->u.expr);
            putchar(')');
            break;

        case kAtom_Plus:
            putchar('+');
            break;

        case kAtom_Minus:
            putchar('-');
            break;

        case kAtom_Multiply:
            putchar('*');
            break;

        case kAtom_Divide:
            putchar('/');
            break;

        case kAtom_Assignment:
            putchar('=');
            break;

        case kAtom_Less:
            putchar('<');
            break;

        case kAtom_Greater:
            putchar('>');
            break;

        case kAtom_LessEqual:
            fputs("<=", stdout);
            break;

        case kAtom_GreaterEqual:
            fputs(">=", stdout);
            break;

        case kAtom_NotEqual:
            fputs("!=", stdout);
            break;

        case kAtom_Equal:
            fputs("==", stdout);
            break;

        default:
            abort();
            break;
    }
}
#endif



////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: SExpression (Symbolic expression)
////////////////////////////////////////////////////////////////////////////////

errno_t SExpression_Create(StackAllocatorRef _Nonnull pAllocator, SExpression* _Nullable * _Nonnull pOutSelf)
{
    SExpression* self = StackAllocator_ClearAlloc(pAllocator, sizeof(SExpression));

    *pOutSelf = self;
    return (self) ? EOK : ENOMEM;
}

void SExpression_AddAtom(SExpression* _Nonnull self, Atom* _Nonnull atom)
{
    if (self->lastAtom) {
        (self->lastAtom)->next = atom;
    }
    else {
        self->atoms = atom;
    }

    self->lastAtom = atom;
}

#ifdef SCRIPT_PRINTING
void SExpression_Print(SExpression* _Nonnull self)
{
    Atom* atom = self->atoms;

    while(atom) {
        Atom_Print(atom);
        atom = atom->next;
    }
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: PExpression (pipe-connected expression)
////////////////////////////////////////////////////////////////////////////////

errno_t PExpression_Create(StackAllocatorRef _Nonnull pAllocator, PExpression* _Nullable * _Nonnull pOutSelf)
{
    PExpression* self = StackAllocator_ClearAlloc(pAllocator, sizeof(PExpression));

    *pOutSelf = self;
    return (self) ? EOK : ENOMEM;
}

void PExpression_AddSExpression(PExpression* _Nonnull self, SExpression* _Nonnull expr)
{
    if (self->lastExpr) {
        (self->lastExpr)->next = expr;
    }
    else {
        self->exprs = expr;
    }

    self->lastExpr = expr;
}

#ifdef SCRIPT_PRINTING
void PExpression_Print(PExpression* _Nonnull self)
{
    SExpression* expr = self->exprs;

    while(expr) {
        SExpression_Print(expr);
        expr = expr->next;
        if (expr) {
            printf(" | ");
        }
    }

    switch (self->terminator) {
        case kToken_Eof:        fputs("<EOF>", stdout); break;
        case kToken_Newline:    fputs("<NL>", stdout); break;
        case kToken_Semicolon:  putchar(';'); break;
        case kToken_Ampersand:  putchar('&'); break;
        default:                abort(); break;
    }
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Block
////////////////////////////////////////////////////////////////////////////////

errno_t Block_Create(StackAllocatorRef _Nonnull pAllocator, Block* _Nullable * _Nonnull pOutSelf)
{
    Block* self = StackAllocator_ClearAlloc(pAllocator, sizeof(Block));

    *pOutSelf = self;
    return (self) ? EOK : ENOMEM;
}

void Block_AddPExpression(Block* _Nonnull self, PExpression* _Nonnull expr)
{
    if (self->lastExpr) {
        (self->lastExpr)->next = expr;
    }
    else {
        self->exprs = expr;
    }
    self->lastExpr = expr;
}

#ifdef SCRIPT_PRINTING
void Block_Print(Block* _Nonnull self)
{
    PExpression* expr = self->exprs;

    while(expr) {
        PExpression_Print(expr);
        expr = expr->next;
        if (expr) {
            putchar('\n');
        }
    }
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Script
////////////////////////////////////////////////////////////////////////////////

errno_t Script_Create(Script* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Script* self;

    try_null(self, calloc(1, sizeof(Script)), errno);
    try(StackAllocator_Create(512, 4096, &self->allocator));

    *pOutSelf = self;
    return EOK;

catch:
    Script_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void Script_Reset(Script* _Nonnull self)
{
    StackAllocator_DeallocAll(self->allocator);
    self->body = NULL;
}

void Script_Destroy(Script* _Nullable self)
{
    if (self) {
        StackAllocator_Destroy(self->allocator);
        self->allocator = NULL;
        self->body = NULL;
        free(self);
    }
}

void Script_SetBlock(Script* _Nonnull self, Block* _Nonnull block)
{
    self->body = block;
}

#ifdef SCRIPT_PRINTING
void Script_Print(Script* _Nonnull self)
{
    Block_Print(self->body);
    putchar('\n');
}
#endif
