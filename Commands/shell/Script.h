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
#include "ConstantPool.h"
#include "RunStack.h"
#include "StackAllocator.h"

//#define SCRIPT_PRINTING 1
struct Block;
struct Expression;

#define AS(__self, __type) ((__type*)__self)


typedef struct VarRef {
    char* _Nonnull  scope;
    char* _Nonnull  name;
} VarRef;

extern errno_t VarRef_Create(StackAllocatorRef _Nonnull pAllocator, const char* str, VarRef* _Nullable * _Nonnull pOutSelf);
extern errno_t VarRef_Init(VarRef* _Nonnull self, const char* str);
#ifdef SCRIPT_PRINTING
extern void VarRef_Print(VarRef* _Nonnull self);
#endif



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
        VarRef* _Nonnull            vref;
    }                       u;
} StringAtom;

extern errno_t StringAtom_CreateWithString(StackAllocatorRef _Nonnull pAllocator, StringAtomType type, const char* _Nonnull str, size_t len, StringAtom* _Nullable * _Nonnull pOutSelf);
extern errno_t StringAtom_CreateWithExpression(StackAllocatorRef _Nonnull pAllocator, struct Expression* _Nonnull expr, StringAtom* _Nullable * _Nonnull pOutSelf);  // Takes ownership of the Expression
extern errno_t StringAtom_CreateWithVarRef(StackAllocatorRef _Nonnull pAllocator, VarRef* _Nonnull vref, StringAtom* _Nullable * _Nonnull pOutSelf);  // Takes ownership of the VarRef
#define StringAtom_GetStringLength(__self) (__self)->u.length
#define StringAtom_GetString(__self) (((const char*)__self) + sizeof(StringAtom))
#define StringAtom_GetMutableString(__self) (((char*)__self) + sizeof(StringAtom))
#ifdef SCRIPT_PRINTING
extern void StringAtom_Print(StringAtom* _Nonnull self);
#endif


typedef struct CompoundString {
    StringAtom* _Nonnull    atoms;
    StringAtom* _Nonnull    lastAtom;
} CompoundString;

extern errno_t CompoundString_Create(StackAllocatorRef _Nonnull pAllocator, CompoundString* _Nullable * _Nonnull pOutSelf);
extern void CompoundString_AddAtom(CompoundString* _Nonnull self, StringAtom* _Nonnull atom);
#ifdef SCRIPT_PRINTING
extern void CompoundString_Print(CompoundString* _Nonnull self);
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
        struct CompoundString* _Nonnull qstring;
        struct Expression* _Nonnull     expr;
        VarRef* _Nonnull                vref;
        int32_t                         i32;
    }                       u;
} Atom;

extern errno_t Atom_CreateWithCharacter(StackAllocatorRef _Nonnull pAllocator, AtomType type, char ch, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf);
extern errno_t Atom_CreateWithString(StackAllocatorRef _Nonnull pAllocator, AtomType type, const char* _Nonnull str, size_t len, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf);
extern errno_t Atom_CreateWithInteger(StackAllocatorRef _Nonnull pAllocator, int32_t i32, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf);
extern errno_t Atom_CreateWithExpression(StackAllocatorRef _Nonnull pAllocator, struct Expression* _Nonnull expr, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf);  // Takes ownership of the Expression
extern errno_t Atom_CreateWithVarRef(StackAllocatorRef _Nonnull pAllocator, VarRef* _Nonnull vref, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf);  // Takes ownership of the VarRef
extern errno_t Atom_CreateWithCompoundString(StackAllocatorRef _Nonnull pAllocator, AtomType type, struct CompoundString* _Nonnull str, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf);  // Takes ownership of the String
#define Atom_GetStringLength(__self) (__self)->u.stringLength
#define Atom_GetString(__self) (((const char*)__self) + sizeof(Atom))
#define Atom_GetMutableString(__self) (((char*)__self) + sizeof(Atom))
#ifdef SCRIPT_PRINTING
extern void Atom_Print(Atom* _Nonnull self);
#endif



typedef enum ExpressionType {
    kExpression_Pipeline,           // BinaryExpression
    kExpression_Disjunction,        // BinaryExpression
    kExpression_Conjunction,        // BinaryExpression
    kExpression_Equal,              // BinaryExpression
    kExpression_NotEqual,           // BinaryExpression
    kExpression_LessEqual,          // BinaryExpression
    kExpression_GreaterEqual,       // BinaryExpression
    kExpression_Less,               // BinaryExpression
    kExpression_Greater,            // BinaryExpression
    kExpression_Addition,           // BinaryExpression
    kExpression_Subtraction,        // BinaryExpression
    kExpression_Multiplication,     // BinaryExpression
    kExpression_Division,           // BinaryExpression
    kExpression_Positive,           // UnaryExpression
    kExpression_Negative,           // UnaryExpression
    kExpression_LogicalInverse,     // UnaryExpression
    kExpression_Parenthesized,      // UnaryExpression
    kExpression_Bool,               // BoolLiteral
    kExpression_Integer,            // IntegerLiteral
    kExpression_String,             // StringLiteral
    kExpression_CompoundString,     // CompoundStringLiteral
    kExpression_VarRef,             // VarRefExpression
    kExpression_Command,            // CommandExpression
    kExpression_If,                 // IfExpression
    kExpression_While,              // WhileExpression
} ExpressionType;

typedef struct Expression {
    int8_t      type;
} Expression;

typedef struct BoolLiteral {
    Expression  super;
    bool        b;
} BoolLiteral;

typedef struct IntegerLiteral {
    Expression  super;
    int32_t     i32;
} IntegerLiteral;

typedef struct StringLiteral {
    Expression  super;
    size_t      length;
    char        characters[1];
} StringLiteral;

typedef struct CompoundStringLiteral {
    Expression      super;
    CompoundString* qstring;
} CompoundStringLiteral;

typedef struct BinaryExpression {
    Expression              super;
    Expression* _Nonnull    lhs;
    Expression* _Nonnull    rhs;
} BinaryExpression;

typedef struct UnaryExpression {
    Expression              super;
    Expression* _Nullable   expr;
} UnaryExpression;

typedef struct VarRefExpression {
    Expression          super;
    VarRef* _Nonnull    vref;
} VarRefExpression;

typedef struct CommandExpression {
    Expression          super;
    Atom* _Nonnull      atoms;
    Atom* _Nonnull      lastAtom;
} CommandExpression;

typedef struct IfExpression {
    Expression              super;
    Expression* _Nonnull    cond;
    struct Block* _Nonnull  thenBlock;
    struct Block* _Nullable elseBlock;
} IfExpression;

typedef struct WhileExpression {
    Expression              super;
    Expression* _Nonnull    cond;
    struct Block* _Nonnull  body;
} WhileExpression;

extern errno_t Expression_CreateBool(StackAllocatorRef _Nonnull pAllocator, bool b, Expression* _Nullable * _Nonnull pOutSelf);
extern errno_t Expression_CreateInteger(StackAllocatorRef _Nonnull pAllocator, int32_t i32, Expression* _Nullable * _Nonnull pOutSelf);
extern errno_t Expression_CreateString(StackAllocatorRef _Nonnull pAllocator, const char* text, size_t len, Expression* _Nullable * _Nonnull pOutSelf);
extern errno_t Expression_CreateCompoundString(StackAllocatorRef _Nonnull pAllocator, CompoundString* _Nonnull qstr, Expression* _Nullable * _Nonnull pOutSelf);
extern errno_t Expression_CreateBinary(StackAllocatorRef _Nonnull pAllocator, ExpressionType type, Expression* _Nonnull lhs, Expression* _Nonnull rhs, Expression* _Nullable * _Nonnull pOutSelf);
extern errno_t Expression_CreateUnary(StackAllocatorRef _Nonnull pAllocator, ExpressionType type, Expression* _Nullable expr, Expression* _Nullable * _Nonnull pOutSelf);
extern errno_t Expression_CreateVarRef(StackAllocatorRef _Nonnull pAllocator, VarRef* _Nonnull vref, Expression* _Nullable * _Nonnull pOutSelf);
extern errno_t Expression_CreateIfThen(StackAllocatorRef _Nonnull pAllocator, Expression* _Nonnull cond, struct Block* _Nonnull thenBlock, struct Block* _Nullable elseBlock, Expression* _Nullable * _Nonnull pOutSelf);
extern errno_t Expression_CreateWhile(StackAllocatorRef _Nonnull pAllocator, Expression* _Nonnull cond, struct Block* _Nonnull body, Expression* _Nullable * _Nonnull pOutSelf);
extern errno_t Expression_CreateCommand(StackAllocatorRef _Nonnull pAllocator, Expression* _Nullable * _Nonnull pOutSelf);
extern void CommandExpression_AddAtom(CommandExpression* _Nonnull self, Atom* _Nonnull atom);
#ifdef SCRIPT_PRINTING
extern void Expression_Print(Expression* _Nonnull self);
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
    ConstantPool* _Nonnull      constantPool;
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
