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

static errno_t Atom_Create(StackAllocatorRef _Nonnull pAllocator, AtomType type, size_t nExtraBytes, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Atom* self = NULL;
    
    try_null(self, StackAllocator_ClearAlloc(pAllocator, sizeof(Atom) + nExtraBytes), ENOMEM);
    self->type = type;
    self->hasLeadingWhitespace = hasLeadingWhitespace;

catch:
    *pOutSelf = self;
    return err;
}

errno_t Atom_CreateWithCharacter(StackAllocatorRef _Nonnull pAllocator, AtomType type, char ch, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Atom* self = NULL;
    
    try(Atom_Create(pAllocator, type, 1 + 1, hasLeadingWhitespace, &self));
    self->u.stringLength = 1;
    
    char* str = Atom_GetMutableString(self);
    str[0] = ch;
    str[1] = '\0';

catch:
    *pOutSelf = self;
    return err;
}

errno_t Atom_CreateWithString(StackAllocatorRef _Nonnull pAllocator, AtomType type, const char* _Nonnull str, size_t len, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Atom* self = NULL;

    try(Atom_Create(pAllocator, type, len + 1, hasLeadingWhitespace, &self));    
    self->u.stringLength = len;

    char* dst = Atom_GetMutableString(self);
    memcpy(dst, str, len);
    dst[len] = '\0';

catch:
    *pOutSelf = self;
    return err;
}

errno_t Atom_CreateWithExpression(StackAllocatorRef _Nonnull pAllocator, Expression* _Nonnull expr, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Atom* self = NULL;

    try(Atom_Create(pAllocator, kAtom_Expression, 0, hasLeadingWhitespace, &self));
    self->u.expr = expr;

catch:
    *pOutSelf = self;
    return err;
}

errno_t Atom_CreateWithVarRef(StackAllocatorRef _Nonnull pAllocator, struct VarRef* _Nonnull vref, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Atom* self = NULL;

    try(Atom_Create(pAllocator, kAtom_VariableReference, 0, hasLeadingWhitespace, &self));
    self->u.vref = vref;

catch:
    *pOutSelf = self;
    return err;
}

errno_t Atom_CreateWithQuotedString(StackAllocatorRef _Nonnull pAllocator, struct QuotedString* _Nonnull str, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Atom* self = NULL;

    try(Atom_Create(pAllocator, kAtom_QuotedString, 0, hasLeadingWhitespace, &self));
    self->u.qstring = str;

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
        case kAtom_BacktickString:
            printf("`%s`", Atom_GetString(self));
            break;

        case kAtom_SingleQuoteString:
            printf("'%s'", Atom_GetString(self));
            break;

        case kAtom_QuotedString:
            QuotedString_Print(self->u.qstring);
            break;

        case kAtom_VariableReference:
            VarRef_Print(self->u.vref);
            break;

        case kAtom_Expression:
            putchar('(');
            Expression_Print(self->u.expr);
            putchar(')');
            break;

        default:
            fputs(Atom_GetString(self), stdout);
            break;
    }
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: VarRef
////////////////////////////////////////////////////////////////////////////////

errno_t VarRef_Create(StackAllocatorRef _Nonnull pAllocator, const char* str, VarRef* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    const char* colon = (const char*)strrchr(str, ':');
    const char* scope = (colon) ? str : "";
    const char* name = (colon) ? colon + 1 : str;
    const size_t scopeLen = (colon) ? colon - str : 0;
    const size_t nameLen = strlen(name);
    VarRef* self = NULL;
    
    try_null(self, StackAllocator_ClearAlloc(pAllocator, sizeof(VarRef) + scopeLen + 1 + nameLen + 1), ENOMEM);
    self->scope = ((char*)self) + sizeof(VarRef);
    self->name = self->scope + scopeLen + 1;

    memcpy(self->scope, scope, scopeLen);
    self->scope[scopeLen] = '\0';
    memcpy(self->name, name, nameLen);
    self->name[nameLen] = '\0';

catch:
    *pOutSelf = self;
    return err;
}

#ifdef SCRIPT_PRINTING
void VarRef_Print(VarRef* _Nonnull self)
{
    putchar('$');
    if (*self->scope != '\0') {
        fputs(self->scope, stdout);
        putchar(':');
    }
    fputs(self->name, stdout);
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Command
////////////////////////////////////////////////////////////////////////////////

errno_t Command_Create(StackAllocatorRef _Nonnull pAllocator, Command* _Nullable * _Nonnull pOutSelf)
{
    Command* self = StackAllocator_ClearAlloc(pAllocator, sizeof(Command));

    *pOutSelf = self;
    return (self) ? EOK : ENOMEM;
}

void Command_AddAtom(Command* _Nonnull self, Atom* _Nonnull atom)
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
void Command_Print(Command* _Nonnull self)
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
// MARK: StringAtom
////////////////////////////////////////////////////////////////////////////////

static errno_t StringAtom_Create(StackAllocatorRef _Nonnull pAllocator, StringAtomType type, size_t nExtraBytes, StringAtom* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    StringAtom* self = NULL;
    
    try_null(self, StackAllocator_ClearAlloc(pAllocator, sizeof(StringAtom) + nExtraBytes), ENOMEM);
    self->type = type;

catch:
    *pOutSelf = self;
    return err;
}

errno_t StringAtom_CreateWithString(StackAllocatorRef _Nonnull pAllocator, StringAtomType type, const char* _Nonnull str, size_t len, StringAtom* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    StringAtom* self = NULL;

    try(StringAtom_Create(pAllocator, type, len + 1, &self));    
    self->u.length = len;

    char* dst = StringAtom_GetMutableString(self);
    memcpy(dst, str, len);
    dst[len] = '\0';

catch:
    *pOutSelf = self;
    return err;
}

errno_t StringAtom_CreateWithExpression(StackAllocatorRef _Nonnull pAllocator, Expression* _Nonnull expr, StringAtom* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    StringAtom* self = NULL;

    try(StringAtom_Create(pAllocator, kStringAtom_Expression, 0, &self));
    self->u.expr = expr;

catch:
    *pOutSelf = self;
    return err;
}

errno_t StringAtom_CreateWithVarRef(StackAllocatorRef _Nonnull pAllocator, struct VarRef* _Nonnull vref, StringAtom* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    StringAtom* self = NULL;

    try(StringAtom_Create(pAllocator, kStringAtom_VariableReference, 0, &self));
    self->u.vref = vref;

catch:
    *pOutSelf = self;
    return err;
}

#ifdef SCRIPT_PRINTING
void StringAtom_Print(StringAtom* _Nonnull self)
{
    switch (self->type) {
        case kStringAtom_VariableReference:
            VarRef_Print(self->u.vref);
            break;

        case kStringAtom_Expression:
            fputs("\\(", stdout);
            Expression_Print(self->u.expr);
            putchar(')');
            break;

        case kStringAtom_EscapeSequence:
            putchar('\\');
            fputs(Atom_GetString(self), stdout);
            break;

        case kStringAtom_Segment:
            fputs(Atom_GetString(self), stdout);
            break;

        default:
            abort();
            break;
    }
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: QuotedString
////////////////////////////////////////////////////////////////////////////////

errno_t QuotedString_Create(StackAllocatorRef _Nonnull pAllocator, bool isBacktick, QuotedString* _Nullable * _Nonnull pOutSelf)
{
    QuotedString* self = StackAllocator_ClearAlloc(pAllocator, sizeof(QuotedString));
    self->isBacktick = isBacktick;

    *pOutSelf = self;
    return (self) ? EOK : ENOMEM;
}

void QuotedString_AddAtom(QuotedString* _Nonnull self, StringAtom* _Nonnull atom)
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
void QuotedString_Print(QuotedString* _Nonnull self)
{
    if (self->isBacktick) {
        fputs("``", stdout);
    }
    else {
        putchar('"');
    }

    StringAtom* atom = self->atoms;
    while(atom) {
        StringAtom_Print(atom);
        atom = atom->next;
    }

    if (self->isBacktick) {
        fputs("``", stdout);
    }
    else {
        putchar('"');
    }
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Expression
////////////////////////////////////////////////////////////////////////////////

errno_t Expression_Create(StackAllocatorRef _Nonnull pAllocator, Expression* _Nullable * _Nonnull pOutSelf)
{
    Expression* self = StackAllocator_ClearAlloc(pAllocator, sizeof(Expression));

    *pOutSelf = self;
    return (self) ? EOK : ENOMEM;
}

void Expression_AddCommand(Expression* _Nonnull self, Command* _Nonnull cmd)
{
    if (self->lastCmd) {
        (self->lastCmd)->next = cmd;
    }
    else {
        self->cmds = cmd;
    }

    self->lastCmd = cmd;
}

#ifdef SCRIPT_PRINTING
void Expression_Print(Expression* _Nonnull self)
{
    Command* cmd = self->cmds;

    while(cmd) {
        Command_Print(cmd);
        cmd = cmd->next;
        if (cmd) {
            fputs(" | ", stdout);
        }
    }
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Statement
////////////////////////////////////////////////////////////////////////////////

errno_t Statement_Create(StackAllocatorRef _Nonnull pAllocator, Statement* _Nullable * _Nonnull pOutSelf)
{
    Statement* self = StackAllocator_ClearAlloc(pAllocator, sizeof(Statement));

    self->type = kStatementType_Null;
    *pOutSelf = self;
    return (self) ? EOK : ENOMEM;
}

void Statement_SetExpression(Statement* _Nonnull self, Expression* _Nonnull expr)
{
    self->type = kStatementType_Expression;
    self->u.expr = expr;
}

#ifdef SCRIPT_PRINTING
void Statement_Print(Statement* _Nonnull self)
{
    switch (self->type) {
        case kStatementType_Expression:
            Expression_Print(self->u.expr);
            break;

        default:
            abort();
            break;
    }

    if (self->isAsync) {
        putchar('&');
    }
    else {
        putchar(';');
    }
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: StatementList
////////////////////////////////////////////////////////////////////////////////

void StatementList_Init(StatementList* _Nonnull self)
{
    self->stmts = NULL;
    self->lastStmt = NULL;
}

void StatementList_AddStatement(StatementList* _Nonnull self, Statement* _Nonnull stmt)
{
    if (self->lastStmt) {
        (self->lastStmt)->next = stmt;
    }
    else {
        self->stmts = stmt;
    }
    self->lastStmt = stmt;
}

#ifdef SCRIPT_PRINTING
void StatementList_Print(StatementList* _Nonnull self)
{
    Statement* stmt = self->stmts;

    while(stmt) {
        Statement_Print(stmt);
        stmt = stmt->next;
        if (stmt) {
            putchar('\n');
        }
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

    StatementList_Init(&self->statements);
    *pOutSelf = self;
    return (self) ? EOK : ENOMEM;
}

#ifdef SCRIPT_PRINTING
void Block_Print(Block* _Nonnull self)
{
    putchar('{');
    StatementList_Print(&self->statements);
    putchar('}');
    putchar('\n');
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
    StatementList_Init(&self->statements);

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
    self->statements.stmts = NULL;
    self->statements.lastStmt = NULL;
}

void Script_Destroy(Script* _Nullable self)
{
    if (self) {
        StackAllocator_Destroy(self->allocator);
        self->allocator = NULL;
        self->statements.stmts = NULL;
        self->statements.lastStmt = NULL;
        free(self);
    }
}

#ifdef SCRIPT_PRINTING
void Script_Print(Script* _Nonnull self)
{
    StatementList_Print(&self->statements);
    putchar('\n');
}
#endif
