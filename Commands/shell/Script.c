//
//  Script.c
//  sh
//
//  Created by Dietmar Planitzer on 1/2/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Script.h"
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


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

errno_t StringAtom_CreateWithVarRef(StackAllocatorRef _Nonnull pAllocator, VarRef* _Nonnull vref, StringAtom* _Nullable * _Nonnull pOutSelf)
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
// MARK: CompoundString
////////////////////////////////////////////////////////////////////////////////

errno_t CompoundString_Create(StackAllocatorRef _Nonnull pAllocator, CompoundString* _Nullable * _Nonnull pOutSelf)
{
    CompoundString* self = StackAllocator_ClearAlloc(pAllocator, sizeof(CompoundString));

    *pOutSelf = self;
    return (self) ? EOK : ENOMEM;
}

void CompoundString_AddAtom(CompoundString* _Nonnull self, StringAtom* _Nonnull atom)
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
void CompoundString_Print(CompoundString* _Nonnull self)
{
    StringAtom* atom = self->atoms;
    while(atom) {
        StringAtom_Print(atom);
        atom = atom->next;
    }
}
#endif


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

errno_t Atom_CreateWithInteger(StackAllocatorRef _Nonnull pAllocator, int32_t i32, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Atom* self = NULL;

    try(Atom_Create(pAllocator, kAtom_Integer, 0, hasLeadingWhitespace, &self));
    self->u.i32 = i32;

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

errno_t Atom_CreateWithVarRef(StackAllocatorRef _Nonnull pAllocator, VarRef* _Nonnull vref, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Atom* self = NULL;

    try(Atom_Create(pAllocator, kAtom_VariableReference, 0, hasLeadingWhitespace, &self));
    self->u.vref = vref;

catch:
    *pOutSelf = self;
    return err;
}

errno_t Atom_CreateWithCompoundString(StackAllocatorRef _Nonnull pAllocator, AtomType type, struct CompoundString* _Nonnull str, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Atom* self = NULL;

    try(Atom_Create(pAllocator, type, 0, hasLeadingWhitespace, &self));
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

        case kAtom_DoubleBacktickString:
            fputs("``", stdout);
            CompoundString_Print(self->u.qstring);
            fputs("``", stdout);
            break;

        case kAtom_SingleQuoteString:
            printf("'%s'", Atom_GetString(self));
            break;

        case kAtom_DoubleQuoteString:
            putchar('"');
            CompoundString_Print(self->u.qstring);
            putchar('"');
            break;

        case kAtom_Integer:
            printf("%d", self->u.i32);
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
// MARK: Expression
////////////////////////////////////////////////////////////////////////////////

errno_t Expression_CreateLiteral(StackAllocatorRef _Nonnull pAllocator, bool hasLeadingWhitespace, const Value* value, Expression* _Nullable * _Nonnull pOutSelf)
{
    LiteralExpression* self = StackAllocator_ClearAlloc(pAllocator, sizeof(LiteralExpression));

    self->super.type = kExpression_Literal;
    self->super.hasLeadingWhitespace = hasLeadingWhitespace;
    self->value = *value;
    *pOutSelf = (Expression*)self;
    return (self) ? EOK : ENOMEM;
}

errno_t Expression_CreateCompoundString(StackAllocatorRef _Nonnull pAllocator, bool hasLeadingWhitespace, CompoundString* _Nonnull str, Expression* _Nullable * _Nonnull pOutSelf)
{
    CompoundStringExpression* self = StackAllocator_ClearAlloc(pAllocator, sizeof(CompoundStringExpression));

    self->super.type = kExpression_CompoundString;
    self->super.hasLeadingWhitespace = hasLeadingWhitespace;
    self->string = str;
    *pOutSelf = (Expression*)self;
    return (self) ? EOK : ENOMEM;
}

errno_t Expression_CreateBinary(StackAllocatorRef _Nonnull pAllocator, bool hasLeadingWhitespace, ExpressionType type, Expression* _Nonnull lhs, Expression* _Nonnull rhs, Expression* _Nullable * _Nonnull pOutSelf)
{
    BinaryExpression* self = StackAllocator_ClearAlloc(pAllocator, sizeof(BinaryExpression));

    self->super.type = type;
    self->super.hasLeadingWhitespace = hasLeadingWhitespace;
    self->lhs = lhs;
    self->rhs = rhs;
    *pOutSelf = (Expression*)self;
    return (self) ? EOK : ENOMEM;
}

errno_t Expression_CreateUnary(StackAllocatorRef _Nonnull pAllocator, bool hasLeadingWhitespace, ExpressionType type, Expression* _Nullable expr, Expression* _Nullable * _Nonnull pOutSelf)
{
    UnaryExpression* self = StackAllocator_ClearAlloc(pAllocator, sizeof(UnaryExpression));

    self->super.type = type;
    self->super.hasLeadingWhitespace = hasLeadingWhitespace;
    self->expr = expr;
    *pOutSelf = (Expression*)self;
    return (self) ? EOK : ENOMEM;
}

errno_t Expression_CreateVarRef(StackAllocatorRef _Nonnull pAllocator, bool hasLeadingWhitespace, VarRef* _Nonnull vref, Expression* _Nullable * _Nonnull pOutSelf)
{
    VarRefExpression* self = StackAllocator_ClearAlloc(pAllocator, sizeof(VarRefExpression));

    self->super.type = kExpression_VarRef;
    self->super.hasLeadingWhitespace = hasLeadingWhitespace;
    self->vref = vref;
    *pOutSelf = (Expression*)self;
    return (self) ? EOK : ENOMEM;
}

errno_t Expression_CreateIfThen(StackAllocatorRef _Nonnull pAllocator, bool hasLeadingWhitespace, Expression* _Nonnull cond, Block* _Nonnull thenBlock, Block* _Nullable elseBlock, Expression* _Nullable * _Nonnull pOutSelf)
{
    IfExpression* self = StackAllocator_ClearAlloc(pAllocator, sizeof(IfExpression));

    self->super.type = kExpression_If;
    self->super.hasLeadingWhitespace = hasLeadingWhitespace;
    self->cond = cond;
    self->thenBlock = thenBlock;
    self->elseBlock = elseBlock;
    *pOutSelf = (Expression*)self;
    return (self) ? EOK : ENOMEM;
}

errno_t Expression_CreateWhile(StackAllocatorRef _Nonnull pAllocator, bool hasLeadingWhitespace, Expression* _Nonnull cond, Block* _Nonnull body, Expression* _Nullable * _Nonnull pOutSelf)
{
    WhileExpression* self = StackAllocator_ClearAlloc(pAllocator, sizeof(WhileExpression));

    self->super.type = kExpression_While;
    self->super.hasLeadingWhitespace = hasLeadingWhitespace;
    self->cond = cond;
    self->body = body;
    *pOutSelf = (Expression*)self;
    return (self) ? EOK : ENOMEM;
}

errno_t Expression_CreateCommand(StackAllocatorRef _Nonnull pAllocator, Expression* _Nullable * _Nonnull pOutSelf)
{
    CommandExpression* self = StackAllocator_ClearAlloc(pAllocator, sizeof(CommandExpression));

    self->super.type = kExpression_Command;
    self->super.hasLeadingWhitespace = true;
    *pOutSelf = (Expression*)self;
    return (self) ? EOK : ENOMEM;
}

void CommandExpression_AddAtom(CommandExpression* _Nonnull self, Atom* _Nonnull atom)
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
void Expression_Print(Expression* _Nonnull self)
{
    static const char* gPrefix[] = {
        "+", "-", "!"
    };
    static const char* gInfix[] = {
        "|", "||", "&&", "==", "!=", "<=", ">=", "<", ">", "+", "-", "*", "/",
    };

    switch (self->type) {
        case kExpression_Command: {
            Atom* atom = AS(self, CommandExpression)->atoms;

            while(atom) {
                Atom_Print(atom);
                atom = atom->next;
            }
            break;
        }

        case kExpression_Pipeline:
        case kExpression_Disjunction:
        case kExpression_Conjunction:
        case kExpression_Equals:
        case kExpression_NotEquals:
        case kExpression_LessEquals:
        case kExpression_GreaterEquals:
        case kExpression_Less:
        case kExpression_Greater:
        case kExpression_Addition:
        case kExpression_Subtraction:
        case kExpression_Multiplication:
        case kExpression_Division:
            Expression_Print(AS(self, BinaryExpression)->lhs);
            printf(" %s", gInfix[self->type - kExpression_Pipeline]);
            Expression_Print(AS(self, BinaryExpression)->rhs);
            break;

        case kExpression_Positive:
        case kExpression_Negative:
        case kExpression_Not:
            fputs(gPrefix[self->type - kExpression_Positive], stdout);
            Expression_Print(AS(self, UnaryExpression)->expr);
            break;

        case kExpression_Parenthesized:
            putchar('(');
            Expression_Print(AS(self, UnaryExpression)->expr);
            putchar(')');
            break;

        case kExpression_Literal:
            Value_Write(&AS(self, LiteralExpression)->value, stdout);
            break;

        case kExpression_CompoundString:
            CompoundString_Print(AS(self, CompoundStringExpression)->string);
            break;

        case kExpression_VarRef:
            VarRef_Print(AS(self, VarRefExpression)->vref);
            break;

        case kExpression_If: {
            IfExpression* ie = AS(self, IfExpression);
            fputs("if ", stdout);
            Expression_Print(ie->cond);
            putchar(' ');
            Block_Print(ie->thenBlock);
            if (ie->elseBlock) {
                fputs(" else ", stdout);
                Block_Print(ie->elseBlock);
            }
            break;
        }

        case kExpression_While: {
            WhileExpression* we = AS(self, WhileExpression);
            fputs("while ", stdout);
            Expression_Print(we->cond);
            putchar(' ');
            Block_Print(we->body);
            break;
        }

        default:
            abort();
            break;
    }
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Statement
////////////////////////////////////////////////////////////////////////////////

errno_t Statement_CreateNull(StackAllocatorRef _Nonnull pAllocator, Statement* _Nullable * _Nonnull pOutSelf)
{
    Statement* self = StackAllocator_ClearAlloc(pAllocator, sizeof(Statement));

    self->type = kStatement_Null;
    *pOutSelf = self;
    return (self) ? EOK : ENOMEM;
}

errno_t Statement_CreateExpression(StackAllocatorRef _Nonnull pAllocator, Expression* _Nonnull expr, Statement* _Nullable * _Nonnull pOutSelf)
{
    ExpressionStatement* self = StackAllocator_ClearAlloc(pAllocator, sizeof(ExpressionStatement));

    self->super.type = kStatement_Expression;
    self->expr = expr;
    *pOutSelf = (Statement*)self;
    return (self) ? EOK : ENOMEM;
}

errno_t Statement_CreateAssignment(StackAllocatorRef _Nonnull pAllocator, Expression* _Nonnull lvalue, Expression* _Nonnull rvalue, Statement* _Nullable * _Nonnull pOutSelf)
{
    AssignmentStatement* self = StackAllocator_ClearAlloc(pAllocator, sizeof(AssignmentStatement));

    self->super.type = kStatement_Assignment;
    self->lvalue = lvalue;
    self->rvalue = rvalue;
    *pOutSelf = (Statement*)self;
    return (self) ? EOK : ENOMEM;
}

errno_t Statement_CreateVarDecl(StackAllocatorRef _Nonnull pAllocator, unsigned int modifiers, VarRef* _Nonnull vref, struct Expression* _Nonnull expr, Statement* _Nullable * _Nonnull pOutSelf)
{
    VarDeclStatement* self = StackAllocator_ClearAlloc(pAllocator, sizeof(VarDeclStatement));

    self->super.type = kStatement_VarDecl;
    self->vref = vref;
    self->expr = expr;
    self->modifiers = modifiers;
    *pOutSelf = (Statement*)self;
    return (self) ? EOK : ENOMEM;
}

#ifdef SCRIPT_PRINTING
void Statement_Print(Statement* _Nonnull self)
{
    switch (self->type) {
        case kStatement_Null:
            break;

        case kStatement_Expression:
            Expression_Print(AS(self, ExpressionStatement)->expr);
            break;

        case kStatement_Assignment:
            Expression_Print(AS(self, AssignmentStatement)->lvalue);
            fputs(" =", stdout);
            Expression_Print(AS(self, AssignmentStatement)->rvalue);
            break;

        case kStatement_VarDecl: {
            VarDeclStatement* decl = AS(self, VarDeclStatement);

            fputs(((decl->modifiers & kVarModifier_Public) != 0) ? "public " : "internal ", stdout);
            fputs(((decl->modifiers & kVarModifier_Mutable) != 0) ? "var " : "let ", stdout);
            VarRef_Print(decl->vref);
            fputs(" =", stdout);
            Expression_Print(decl->expr);
            break;
        }
            
        default:
            abort();
            break;
    }

    putchar((self->isAsync) ? '&' : ';');
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
    fputs("}\n", stdout);
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
    try(ConstantsPool_Create(&self->constantsPool));
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
    // keep the constant pool around to allow for reuse
}

void Script_Destroy(Script* _Nullable self)
{
    if (self) {
        StackAllocator_Destroy(self->allocator);
        self->allocator = NULL;

        ConstantsPool_Destroy(self->constantsPool);
        self->constantsPool = NULL;

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
