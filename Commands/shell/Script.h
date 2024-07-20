//
//  Script.h
//  sh
//
//  Created by Dietmar Planitzer on 1/2/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Script_h
#define Script_h

#include <System/System.h>
#include "Lexer.h"
#include "RunStack.h"
#include "StackAllocator.h"

#define SCRIPT_PRINTING 1
struct Expression;
struct QuotedString;
struct VarRef;

#define AS(__self, __type) ((__type*)__self)


typedef enum StringAtomType {
    kStringAtom_Segment,            // u.string
    kStringAtom_EscapeSequence,     // u.string
    kStringAtom_Expression,         // u.expr
    kStringAtom_VariableReference   // u.vref
} StringAtomType;

typedef struct StringAtom {
    struct StringAtom* _Nullable    next;
    int8_t                          type;
    int8_t                          reserved[3];
    union {
        size_t                      length;     // nul-terminated string follows the atom structure
        struct Expression* _Nonnull expr;
        struct VarRef* _Nonnull     vref;
    }                       u;
} StringAtom;

extern errno_t StringAtom_CreateWithString(StackAllocatorRef _Nonnull pAllocator, StringAtomType type, const char* _Nonnull str, size_t len, StringAtom* _Nullable * _Nonnull pOutSelf);
extern errno_t StringAtom_CreateWithExpression(StackAllocatorRef _Nonnull pAllocator, struct Expression* _Nonnull expr, StringAtom* _Nullable * _Nonnull pOutSelf);  // Takes ownership of the Expression
extern errno_t StringAtom_CreateWithVarRef(StackAllocatorRef _Nonnull pAllocator, struct VarRef* _Nonnull vref, StringAtom* _Nullable * _Nonnull pOutSelf);  // Takes ownership of the VarRef
#define StringAtom_GetStringLength(__self) (__self)->u.length
#define StringAtom_GetString(__self) (((const char*)__self) + sizeof(StringAtom))
#define StringAtom_GetMutableString(__self) (((char*)__self) + sizeof(StringAtom))
#ifdef SCRIPT_PRINTING
extern void StringAtom_Print(StringAtom* _Nonnull self);
#endif


typedef struct QuotedString {
    StringAtom* _Nonnull    atoms;
    StringAtom* _Nonnull    lastAtom;
} QuotedString;

extern errno_t QuotedString_Create(StackAllocatorRef _Nonnull pAllocator, QuotedString* _Nullable * _Nonnull pOutSelf);
extern void QuotedString_AddAtom(QuotedString* _Nonnull self, StringAtom* _Nonnull atom);
#ifdef SCRIPT_PRINTING
extern void QuotedString_Print(QuotedString* _Nonnull self);
#endif



typedef enum AtomType {
    kAtom_BacktickString,           // u.stringLength
    kAtom_SingleQuoteString,        // u.stringLength
    kAtom_Identifier,               // u.stringLength
    kAtom_Integer,                  // u.i32

    kAtom_DoubleBacktickString,     // u.qstring
    kAtom_DoubleQuoteString,        // u.qstring
    kAtom_VariableReference,        // u.vref
    kAtom_Expression,               // u.expr
} AtomType;

typedef struct Atom {
    struct Atom* _Nullable  next;
    int8_t                  type;
    int8_t                  reserved[2];
    bool                    hasLeadingWhitespace;
    union {
        size_t                          stringLength;   // nul-terminated string follows the atom structure
        struct QuotedString* _Nonnull   qstring;
        struct Expression* _Nonnull     expr;
        struct VarRef* _Nonnull         vref;
        int32_t                         i32;
    }                       u;
} Atom;

extern errno_t Atom_CreateWithCharacter(StackAllocatorRef _Nonnull pAllocator, AtomType type, char ch, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf);
extern errno_t Atom_CreateWithString(StackAllocatorRef _Nonnull pAllocator, AtomType type, const char* _Nonnull str, size_t len, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf);
extern errno_t Atom_CreateWithInteger(StackAllocatorRef _Nonnull pAllocator, int32_t i32, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf);
extern errno_t Atom_CreateWithExpression(StackAllocatorRef _Nonnull pAllocator, struct Expression* _Nonnull expr, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf);  // Takes ownership of the Expression
extern errno_t Atom_CreateWithVarRef(StackAllocatorRef _Nonnull pAllocator, struct VarRef* _Nonnull vref, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf);  // Takes ownership of the VarRef
extern errno_t Atom_CreateWithQuotedString(StackAllocatorRef _Nonnull pAllocator, AtomType type, struct QuotedString* _Nonnull str, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf);  // Takes ownership of the String
#define Atom_GetStringLength(__self) (__self)->u.stringLength
#define Atom_GetString(__self) (((const char*)__self) + sizeof(Atom))
#define Atom_GetMutableString(__self) (((char*)__self) + sizeof(Atom))
#ifdef SCRIPT_PRINTING
extern void Atom_Print(Atom* _Nonnull self);
#endif



typedef struct Command {
    struct Command* _Nullable   next;
    Atom* _Nonnull              atoms;
    Atom* _Nonnull              lastAtom;
} Command;

extern errno_t Command_Create(StackAllocatorRef _Nonnull pAllocator, Command* _Nullable * _Nonnull pOutSelf);
extern void Command_AddAtom(Command* _Nonnull self, Atom* _Nonnull atom);
#ifdef SCRIPT_PRINTING
extern void Command_Print(Command* _Nonnull self);
#endif



typedef struct Expression {
    Command* _Nonnull   cmds;
    Command* _Nonnull   lastCmd;
} Expression;

extern errno_t Expression_Create(StackAllocatorRef _Nonnull pAllocator, Expression* _Nullable * _Nonnull pOutSelf);
extern void Expression_AddCommand(Expression* _Nonnull self, Command* _Nonnull cmd);
#ifdef SCRIPT_PRINTING
extern void Expression_Print(Expression* _Nonnull self);
#endif



typedef struct VarRef {
    char* _Nonnull  scope;
    char* _Nonnull  name;
} VarRef;

extern errno_t VarRef_Create(StackAllocatorRef _Nonnull pAllocator, const char* str, VarRef* _Nullable * _Nonnull pOutSelf);
extern errno_t VarRef_Init(VarRef* _Nonnull self, const char* str);
#ifdef SCRIPT_PRINTING
extern void VarRef_Print(VarRef* _Nonnull self);
#endif



typedef enum StatementType {
    kStatement_Null,            // Statement
    kStatement_Expression,      // ExpressionStatement
    kStatement_Assignment,      // AssignmentStatement
    kStatement_VarDecl,         // VarDeclStatement
} StatementType;

typedef struct Statement {
    struct Statement* _Nullable next;
    int8_t                      type;
    bool                        isAsync;    // '&' -> true and ';' | '\n' -> false
} Statement;

typedef struct ExpressionStatement {
    Statement               super;
    Expression* _Nonnull    expr;
} ExpressionStatement;

typedef struct AssignmentStatement {
    Statement               super;
    Expression* _Nonnull    lvalue;
    Expression* _Nonnull    rvalue;
} AssignmentStatement;

typedef struct VarDeclStatement {
    Statement                   super;
    VarRef* _Nonnull            vref;
    struct Expression* _Nonnull expr;
    unsigned int                modifiers;
} VarDeclStatement;

extern errno_t Statement_CreateNull(StackAllocatorRef _Nonnull pAllocator, Statement* _Nullable * _Nonnull pOutSelf);
extern errno_t Statement_CreateExpression(StackAllocatorRef _Nonnull pAllocator, Expression* _Nonnull expr, Statement* _Nullable * _Nonnull pOutSelf);
extern errno_t Statement_CreateAssignment(StackAllocatorRef _Nonnull pAllocator, Expression* _Nonnull lvalue, Expression* _Nonnull rvalue, Statement* _Nullable * _Nonnull pOutSelf);
extern errno_t Statement_CreateVarDecl(StackAllocatorRef _Nonnull pAllocator, unsigned int modifiers, VarRef* _Nonnull vref, struct Expression* _Nonnull expr, Statement* _Nullable * _Nonnull pOutSelf);
#ifdef SCRIPT_PRINTING
extern void Statement_Print(Statement* _Nonnull self);
#endif



typedef struct StatementList {
    Statement* _Nullable    stmts;
    Statement* _Nullable    lastStmt;
} StatementList;

extern void StatementList_Init(StatementList* _Nonnull self);
extern void StatementList_AddStatement(StatementList* _Nonnull self, Statement* _Nonnull stmt);
#ifdef SCRIPT_PRINTING
extern void StatementList_Print(StatementList* _Nonnull self);
#endif



typedef struct Block {
    StatementList   statements;
} Block;

extern errno_t Block_Create(StackAllocatorRef _Nonnull pAllocator, Block* _Nullable * _Nonnull pOutSelf);
#ifdef SCRIPT_PRINTING
extern void Block_Print(Block* _Nonnull self);
#endif



typedef struct Script {
    StatementList               statements;
    StackAllocatorRef _Nonnull  allocator;
} Script;

// The script manages a stack allocator which is used to store all nodes in the
// AST that the script manages.
extern errno_t Script_Create(Script* _Nullable * _Nonnull pOutSelf);
extern void Script_Destroy(Script* _Nullable self);
extern void Script_Reset(Script* _Nonnull self);
#ifdef SCRIPT_PRINTING
extern void Script_Print(Script* _Nonnull self);
#endif

#endif  /* Script_h */
