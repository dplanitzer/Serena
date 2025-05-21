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
// MARK: Segment
////////////////////////////////////////////////////////////////////////////////

errno_t Segment_CreateLiteral(StackAllocatorRef _Nonnull pAllocator, SegmentType type, const Value* _Nonnull value, Segment* _Nullable * _Nonnull pOutSelf)
{
    LiteralSegment* self = StackAllocator_ClearAlloc(pAllocator, sizeof(LiteralSegment));

    self->super.type = type;
    Value_InitCopy(&self->value, value);
    *pOutSelf = (Segment*)self;
    return (self) ? EOK : ENOMEM;
}

errno_t Segment_CreateArithmeticExpression(StackAllocatorRef _Nonnull pAllocator, Arithmetic* _Nonnull expr, Segment* _Nullable * _Nonnull pOutSelf)
{
    ArithmeticSegment* self = StackAllocator_ClearAlloc(pAllocator, sizeof(ArithmeticSegment));

    self->super.type = kSegment_ArithmeticExpression;
    self->expr = expr;
    *pOutSelf = (Segment*)self;
    return (self) ? EOK : ENOMEM;
}

errno_t Segment_CreateVarRef(StackAllocatorRef _Nonnull pAllocator, VarRef* _Nonnull vref, Segment* _Nullable * _Nonnull pOutSelf)
{
    VarRefSegment* self = StackAllocator_ClearAlloc(pAllocator, sizeof(VarRefSegment));

    self->super.type = kSegment_VarRef;
    self->vref = vref;
    *pOutSelf = (Segment*)self;
    return (self) ? EOK : ENOMEM;
}

#ifdef SCRIPT_PRINTING
void Segment_Print(Segment* _Nonnull self)
{
    switch (self->type) {
        case kSegment_VarRef:
            VarRef_Print(AS(self, VarRefSegment)->vref);
            break;

        case kSegment_ArithmeticExpression:
            fputs("\\(", stdout);
            Arithmetic_Print(AS(self, ArithmeticSegment)->expr);
            putchar(')');
            break;

        case kSegment_EscapeSequence:
            putchar('\\');
            // fall through

        case kSegment_String:
            Value_Write(&AS(self, LiteralSegment)->value, stdout);
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

void CompoundString_AddSegment(CompoundString* _Nonnull self, Segment* _Nonnull seg)
{
    if (self->lastSeg) {
        (self->lastSeg)->next = seg;
    }
    else {
        self->segs = seg;
    }

    self->lastSeg = seg;
}

#ifdef SCRIPT_PRINTING
void CompoundString_Print(CompoundString* _Nonnull self)
{
    Segment* seg = self->segs;

    while(seg) {
        Segment_Print(seg);
        seg = seg->next;
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

errno_t Atom_CreateWithArithmeticExpression(StackAllocatorRef _Nonnull pAllocator, Arithmetic* _Nonnull expr, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Atom* self = NULL;

    try(Atom_Create(pAllocator, kAtom_ArithmeticExpression, 0, hasLeadingWhitespace, &self));
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

        case kAtom_ArithmeticExpression:
            putchar('(');
            Arithmetic_Print(self->u.expr);
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
// MARK: ArithmeticExpression
////////////////////////////////////////////////////////////////////////////////

Arithmetic* _Nonnull Arithmetic_CreateLiteral(StackAllocatorRef _Nonnull pAllocator, bool hasLeadingWhitespace, const Value* value)
{
    LiteralArithmetic* self = StackAllocator_ClearAlloc(pAllocator, sizeof(LiteralArithmetic));

    self->super.type = kArithmetic_Literal;
    self->super.hasLeadingWhitespace = hasLeadingWhitespace;
    Value_InitCopy(&self->value, value);
    return (Arithmetic*)self;
}

Arithmetic* _Nonnull Arithmetic_CreateCompoundString(StackAllocatorRef _Nonnull pAllocator, bool hasLeadingWhitespace, CompoundString* _Nonnull str)
{
    CompoundStringArithmetic* self = StackAllocator_ClearAlloc(pAllocator, sizeof(CompoundStringArithmetic));

    self->super.type = kArithmetic_CompoundString;
    self->super.hasLeadingWhitespace = hasLeadingWhitespace;
    self->string = str;
    return (Arithmetic*)self;
}

Arithmetic* _Nonnull Arithmetic_CreateBinary(StackAllocatorRef _Nonnull pAllocator, bool hasLeadingWhitespace, ArithmeticType type, Arithmetic* _Nonnull lhs, Arithmetic* _Nonnull rhs)
{
    BinaryArithmetic* self = StackAllocator_ClearAlloc(pAllocator, sizeof(BinaryArithmetic));

    self->super.type = type;
    self->super.hasLeadingWhitespace = hasLeadingWhitespace;
    self->lhs = lhs;
    self->rhs = rhs;
    return (Arithmetic*)self;
}

Arithmetic* _Nonnull Arithmetic_CreateUnary(StackAllocatorRef _Nonnull pAllocator, bool hasLeadingWhitespace, ArithmeticType type, Arithmetic* _Nullable expr)
{
    UnaryArithmetic* self = StackAllocator_ClearAlloc(pAllocator, sizeof(UnaryArithmetic));

    self->super.type = type;
    self->super.hasLeadingWhitespace = hasLeadingWhitespace;
    self->expr = expr;
    return (Arithmetic*)self;
}

Arithmetic* _Nonnull Arithmetic_CreateVarRef(StackAllocatorRef _Nonnull pAllocator, bool hasLeadingWhitespace, VarRef* _Nonnull vref)
{
    VarRefArithmetic* self = StackAllocator_ClearAlloc(pAllocator, sizeof(VarRefArithmetic));

    self->super.type = kArithmetic_VarRef;
    self->super.hasLeadingWhitespace = hasLeadingWhitespace;
    self->vref = vref;
    return (Arithmetic*)self;
}

Arithmetic* _Nonnull Arithmetic_CreateIfThen(StackAllocatorRef _Nonnull pAllocator, bool hasLeadingWhitespace, Arithmetic* _Nonnull cond, Block* _Nonnull thenBlock, Block* _Nullable elseBlock)
{
    IfArithmetic* self = StackAllocator_ClearAlloc(pAllocator, sizeof(IfArithmetic));

    self->super.type = kArithmetic_If;
    self->super.hasLeadingWhitespace = hasLeadingWhitespace;
    self->cond = cond;
    self->thenBlock = thenBlock;
    self->elseBlock = elseBlock;
    return (Arithmetic*)self;
}

Arithmetic* _Nonnull Arithmetic_CreateWhile(StackAllocatorRef _Nonnull pAllocator, bool hasLeadingWhitespace, Arithmetic* _Nonnull cond, Block* _Nonnull body)
{
    WhileArithmetic* self = StackAllocator_ClearAlloc(pAllocator, sizeof(WhileArithmetic));

    self->super.type = kArithmetic_While;
    self->super.hasLeadingWhitespace = hasLeadingWhitespace;
    self->cond = cond;
    self->body = body;
    return (Arithmetic*)self;
}

Arithmetic* _Nonnull Arithmetic_CreateCommand(StackAllocatorRef _Nonnull pAllocator)
{
    CommandArithmetic* self = StackAllocator_ClearAlloc(pAllocator, sizeof(CommandArithmetic));

    self->super.type = kArithmetic_Command;
    self->super.hasLeadingWhitespace = true;
    return (Arithmetic*)self;
}

void CommandArithmetic_AddAtom(CommandArithmetic* _Nonnull self, Atom* _Nonnull atom)
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
void Arithmetic_Print(Arithmetic* _Nonnull self)
{
    static const char* gPrefix[] = {
        "+", "-", "!"
    };
    static const char* gInfix[] = {
        "|", "||", "&&", "==", "!=", "<=", ">=", "<", ">", "+", "-", "*", "/",
    };

    switch (self->type) {
        case kArithmetic_Command: {
            Atom* atom = AS(self, CommandArithmetic)->atoms;

            while(atom) {
                Atom_Print(atom);
                atom = atom->next;
            }
            break;
        }

        case kArithmetic_Pipeline:
        case kArithmetic_Disjunction:
        case kArithmetic_Conjunction:
        case kArithmetic_Equals:
        case kArithmetic_NotEquals:
        case kArithmetic_LessEquals:
        case kArithmetic_GreaterEquals:
        case kArithmetic_Less:
        case kArithmetic_Greater:
        case kArithmetic_Addition:
        case kArithmetic_Subtraction:
        case kArithmetic_Multiplication:
        case kArithmetic_Division:
        case kArithmetic_Modulo:
            Arithmetic_Print(AS(self, BinaryArithmetic)->lhs);
            printf(" %s", gInfix[self->type - kArithmetic_Pipeline]);
            Arithmetic_Print(AS(self, BinaryArithmetic)->rhs);
            break;

        case kArithmetic_Positive:
        case kArithmetic_Negative:
        case kArithmetic_Not:
            fputs(gPrefix[self->type - kArithmetic_Positive], stdout);
            Arithmetic_Print(AS(self, UnaryArithmetic)->expr);
            break;

        case kArithmetic_Parenthesized:
            putchar('(');
            Arithmetic_Print(AS(self, UnaryArithmetic)->expr);
            putchar(')');
            break;

        case kArithmetic_Literal:
            Value_Write(&AS(self, LiteralArithmetic)->value, stdout);
            break;

        case kArithmetic_CompoundString:
            CompoundString_Print(AS(self, CompoundStringArithmetic)->string);
            break;

        case kArithmetic_VarRef:
            VarRef_Print(AS(self, VarRefArithmetic)->vref);
            break;

        case kArithmetic_If: {
            IfArithmetic* ie = AS(self, IfArithmetic);
            fputs("if ", stdout);
            Arithmetic_Print(ie->cond);
            putchar(' ');
            Block_Print(ie->thenBlock);
            if (ie->elseBlock) {
                fputs(" else ", stdout);
                Block_Print(ie->elseBlock);
            }
            break;
        }

        case kArithmetic_While: {
            WhileArithmetic* we = AS(self, WhileArithmetic);
            fputs("while ", stdout);
            Arithmetic_Print(we->cond);
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
// MARK: Expression
////////////////////////////////////////////////////////////////////////////////

Expression* _Nonnull Expression_CreateNull(StackAllocatorRef _Nonnull pAllocator)
{
    Expression* self = StackAllocator_ClearAlloc(pAllocator, sizeof(Expression));

    self->type = kExpression_Null;
    return (Expression*)self;
}

Expression* _Nonnull Expression_CreateArithmeticExpression(StackAllocatorRef _Nonnull pAllocator, Arithmetic* _Nonnull expr)
{
    ArithmeticExpression* self = StackAllocator_ClearAlloc(pAllocator, sizeof(ArithmeticExpression));

    self->super.type = kExpression_ArithmeticExpression;
    self->expr = expr;
    return (Expression*)self;
}

Expression* _Nonnull Expression_CreateAssignment(StackAllocatorRef _Nonnull pAllocator, Arithmetic* _Nonnull lvalue, Arithmetic* _Nonnull rvalue)
{
    AssignmentExpression* self = StackAllocator_ClearAlloc(pAllocator, sizeof(AssignmentExpression));

    self->super.type = kExpression_Assignment;
    self->lvalue = lvalue;
    self->rvalue = rvalue;
    return (Expression*)self;
}

Expression* _Nonnull Expression_CreateVarDecl(StackAllocatorRef _Nonnull pAllocator, unsigned int modifiers, VarRef* _Nonnull vref, struct Arithmetic* _Nonnull expr)
{
    VarDeclExpression* self = StackAllocator_ClearAlloc(pAllocator, sizeof(VarDeclExpression));

    self->super.type = kExpression_VarDecl;
    self->vref = vref;
    self->expr = expr;
    self->modifiers = modifiers;
    return (Expression*)self;
}

Expression* _Nonnull Expression_CreateBreak(StackAllocatorRef _Nonnull pAllocator, Arithmetic* _Nonnull expr)
{
    BreakExpression* self = StackAllocator_ClearAlloc(pAllocator, sizeof(BreakExpression));

    self->super.type = kExpression_Break;
    self->expr = expr;
    return (Expression*)self;
}

Expression* _Nonnull Expression_CreateContinue(StackAllocatorRef _Nonnull pAllocator)
{
    ContinueExpression* self = StackAllocator_ClearAlloc(pAllocator, sizeof(ContinueExpression));

    self->super.type = kExpression_Continue;
    return (Expression*)self;
}

#ifdef SCRIPT_PRINTING
void Expression_Print(Expression* _Nonnull self)
{
    switch (self->type) {
        case kExpression_Null:
            break;

        case kExpression_ArithmeticExpression:
            Arithmetic_Print(AS(self, ArithmeticExpression)->expr);
            break;

        case kExpression_Assignment:
            Arithmetic_Print(AS(self, AssignmentExpression)->lvalue);
            fputs(" =", stdout);
            Arithmetic_Print(AS(self, AssignmentExpression)->rvalue);
            break;

        case kExpression_VarDecl: {
            VarDeclExpression* decl = AS(self, VarDeclExpression);

            fputs(((decl->modifiers & kVarModifier_Public) != 0) ? "public " : "internal ", stdout);
            fputs(((decl->modifiers & kVarModifier_Mutable) != 0) ? "var " : "let ", stdout);
            VarRef_Print(decl->vref);
            fputs(" =", stdout);
            Arithmetic_Print(decl->expr);
            break;
        }
        
        case kExpression_Break: {
            BreakExpression* be = AS(self, BreakExpression);

            fputs("break");
            if (be->expr) {
                Arithmetic_Print(be->expr);
            } else {
                putchar('\n');
            }
            break;
        }

        case kExpression_Continue:
            puts("continue");
            break;

        default:
            abort();
            break;
    }

    putchar((self->isAsync) ? '&' : ';');
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: ExpressionList
////////////////////////////////////////////////////////////////////////////////

void ExpressionList_Init(ExpressionList* _Nonnull self)
{
    self->exprs = NULL;
    self->lastExpr = NULL;
}

void ExpressionList_AddExpression(ExpressionList* _Nonnull self, Expression* _Nonnull expr)
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
void ExpressionList_Print(ExpressionList* _Nonnull self)
{
    Expression* expr = self->exprs;

    while(expr) {
        Expression_Print(expr);
        expr = expr->next;
        if (expr) {
            putchar('\n');
        }
    }
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Block
////////////////////////////////////////////////////////////////////////////////

Block* _Nonnull Block_Create(StackAllocatorRef _Nonnull pAllocator)
{
    Block* self = StackAllocator_ClearAlloc(pAllocator, sizeof(Block));

    ExpressionList_Init(&self->exprs);
    return self;
}

#ifdef SCRIPT_PRINTING
void Block_Print(Block* _Nonnull self)
{
    putchar('{');
    ExpressionList_Print(&self->exprs);
    fputs("}\n", stdout);
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Script
////////////////////////////////////////////////////////////////////////////////

Script* _Nonnull Script_Create(void)
{
    Script* self = calloc(1, sizeof(Script));

    self->allocator = StackAllocator_Create(512, 4096);
    self->constantsPool = ConstantsPool_Create();
    ExpressionList_Init(&self->exprs);

    return self;
}

void Script_Reset(Script* _Nonnull self)
{
    StackAllocator_DeallocAll(self->allocator);
    self->exprs.exprs = NULL;
    self->exprs.lastExpr = NULL;
    // keep the constant pool around to allow for reuse
}

void Script_Destroy(Script* _Nullable self)
{
    if (self) {
        StackAllocator_Destroy(self->allocator);
        self->allocator = NULL;

        ConstantsPool_Destroy(self->constantsPool);
        self->constantsPool = NULL;

        self->exprs.exprs = NULL;
        self->exprs.lastExpr = NULL;
        free(self);
    }
}

#ifdef SCRIPT_PRINTING
void Script_Print(Script* _Nonnull self)
{
    ExpressionList_Print(&self->exprs);
    putchar('\n');
}
#endif
