//
//  Script.h
//  sh
//
//  Created by Dietmar Planitzer on 1/2/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Script_h
#define Script_h

#include "Lexer.h"
#include "ConstantsPool.h"
#include "RunStack.h"
#include "StackAllocator.h"
#include "Value.h"

//#define SCRIPT_PRINTING 1
struct Block;
struct Arithmetic;

#define AS(__self, __type) ((__type*)__self)


typedef struct VarRef {
    char* _Nonnull  scope;
    char* _Nonnull  name;
} VarRef;

extern VarRef* _Nonnull VarRef_Create(StackAllocatorRef _Nonnull pAllocator, const char* str);
#ifdef SCRIPT_PRINTING
extern void VarRef_Print(VarRef* _Nonnull self);
#endif



typedef enum SegmentType {
    kSegment_String,                // LiteralSegment
    kSegment_EscapeSequence,        // LiteralSegment
    kSegment_ArithmeticExpression,  // ArithmeticSegment
    kSegment_VarRef                 // VarRefSegment
} SegmentType;

typedef struct Segment {
    struct Segment* _Nullable   next;
    int8_t                      type;
} Segment;

typedef struct LiteralSegment {
    Segment super;
    Value   value;
} LiteralSegment;

typedef struct ArithmeticSegment {
    Segment                     super;
    struct Arithmetic* _Nonnull expr;
} ArithmeticSegment;

typedef struct VarRefSegment {
    Segment             super;
    VarRef* _Nonnull    vref;
} VarRefSegment;

extern Segment* _Nonnull Segment_CreateLiteral(StackAllocatorRef _Nonnull pAllocator, SegmentType type, const Value* _Nonnull value);
extern Segment* _Nonnull Segment_CreateArithmeticExpression(StackAllocatorRef _Nonnull pAllocator, struct Arithmetic* _Nonnull expr);  // Takes ownership of the ArithmeticExpression
extern Segment* _Nonnull Segment_CreateVarRef(StackAllocatorRef _Nonnull pAllocator, VarRef* _Nonnull vref);  // Takes ownership of the VarRef
#ifdef SCRIPT_PRINTING
extern void Segment_Print(Segment* _Nonnull self);
#endif



typedef struct CompoundString {
    Segment* _Nonnull   segs;
    Segment* _Nonnull   lastSeg;
} CompoundString;

extern CompoundString* _Nonnull CompoundString_Create(StackAllocatorRef _Nonnull pAllocator);
extern void CompoundString_AddSegment(CompoundString* _Nonnull self, Segment* _Nonnull seg);
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
    kAtom_ArithmeticExpression,     // u.expr
} AtomType;

typedef struct Atom {
    struct Atom* _Nullable  next;
    int8_t                  type;
    int8_t                  reserved[2];
    bool                    hasLeadingWhitespace;
    union {
        size_t                          stringLength;   // nul-terminated string follows the atom structure
        struct CompoundString* _Nonnull qstring;
        struct Arithmetic* _Nonnull     expr;
        VarRef* _Nonnull                vref;
        int32_t                         i32;
    }                       u;
} Atom;

extern Atom* _Nonnull Atom_CreateWithCharacter(StackAllocatorRef _Nonnull pAllocator, AtomType type, char ch, bool hasLeadingWhitespace);
extern Atom* _Nonnull Atom_CreateWithString(StackAllocatorRef _Nonnull pAllocator, AtomType type, const char* _Nonnull str, size_t len, bool hasLeadingWhitespace);
extern Atom* _Nonnull Atom_CreateWithInteger(StackAllocatorRef _Nonnull pAllocator, int32_t i32, bool hasLeadingWhitespace);
extern Atom* _Nonnull Atom_CreateWithArithmeticExpression(StackAllocatorRef _Nonnull pAllocator, struct Arithmetic* _Nonnull expr, bool hasLeadingWhitespace);  // Takes ownership of the ArithmeticExpression
extern Atom* _Nonnull Atom_CreateWithVarRef(StackAllocatorRef _Nonnull pAllocator, VarRef* _Nonnull vref, bool hasLeadingWhitespace);  // Takes ownership of the VarRef
extern Atom* _Nonnull Atom_CreateWithCompoundString(StackAllocatorRef _Nonnull pAllocator, AtomType type, struct CompoundString* _Nonnull str, bool hasLeadingWhitespace);  // Takes ownership of the String
#define Atom_GetStringLength(__self) (__self)->u.stringLength
#define Atom_GetString(__self) (((const char*)__self) + sizeof(Atom))
#define Atom_GetMutableString(__self) (((char*)__self) + sizeof(Atom))
#ifdef SCRIPT_PRINTING
extern void Atom_Print(Atom* _Nonnull self);
#endif



typedef enum ArithmeticType {
    kArithmetic_Pipeline,           // BinaryArithmetic
    kArithmetic_Disjunction,        // BinaryArithmetic
    kArithmetic_Conjunction,        // BinaryArithmetic
    kArithmetic_Equals,             // BinaryArithmetic     (kBinaryOp_Equals)
    kArithmetic_NotEquals,          // BinaryArithmetic     .
    kArithmetic_LessEquals,         // BinaryArithmetic     .
    kArithmetic_GreaterEquals,      // BinaryArithmetic     .
    kArithmetic_Less,               // BinaryArithmetic     .
    kArithmetic_Greater,            // BinaryArithmetic     .
    kArithmetic_Addition,           // BinaryArithmetic     .
    kArithmetic_Subtraction,        // BinaryArithmetic     .
    kArithmetic_Multiplication,     // BinaryArithmetic     .
    kArithmetic_Division,           // BinaryArithmetic     .
    kArithmetic_Modulo,             // BinaryArithmetic     (kBinaryOp_Modulo)
    kArithmetic_Parenthesized,      // UnaryArithmetic
    kArithmetic_Positive,           // UnaryArithmetic
    kArithmetic_Negative,           // UnaryArithmetic      (kUnaryOp_Negative)
    kArithmetic_Not,                // UnaryArithmetic      (kUnaryOp_Not)
    kArithmetic_Literal,            // LiteralArithmetic
    kArithmetic_CompoundString,     // CompoundStringArithmetic
    kArithmetic_VarRef,             // VarRefArithmetic
    kArithmetic_Command,            // CommandArithmetic
    kArithmetic_If,                 // IfArithmetic
    kArithmetic_While,              // WhileArithmetic
} ArithmeticType;

typedef struct Arithmetic {
    int8_t      type;
    bool        hasLeadingWhitespace;
} Arithmetic;

typedef struct LiteralArithmetic {
    Arithmetic  super;
    Value       value;
} LiteralArithmetic;

typedef struct CompoundStringArithmetic {
    Arithmetic      super;
    CompoundString* string;
} CompoundStringArithmetic;

typedef struct BinaryArithmetic {
    Arithmetic              super;
    Arithmetic* _Nonnull    lhs;
    Arithmetic* _Nonnull    rhs;
} BinaryArithmetic;

typedef struct UnaryArithmetic {
    Arithmetic              super;
    Arithmetic* _Nullable   expr;
} UnaryArithmetic;

typedef struct VarRefArithmetic {
    Arithmetic          super;
    VarRef* _Nonnull    vref;
} VarRefArithmetic;

typedef struct CommandArithmetic {
    Arithmetic          super;
    Atom* _Nonnull      atoms;
    Atom* _Nonnull      lastAtom;
} CommandArithmetic;

typedef struct IfArithmetic {
    Arithmetic              super;
    Arithmetic* _Nonnull    cond;
    struct Block* _Nonnull  thenBlock;
    struct Block* _Nullable elseBlock;
} IfArithmetic;

typedef struct WhileArithmetic {
    Arithmetic              super;
    Arithmetic* _Nonnull    cond;
    struct Block* _Nonnull  body;
} WhileArithmetic;

extern Arithmetic* _Nonnull Arithmetic_CreateLiteral(StackAllocatorRef _Nonnull pAllocator, bool hasLeadingWhitespace, const Value* value);
extern Arithmetic* _Nonnull Arithmetic_CreateCompoundString(StackAllocatorRef _Nonnull pAllocator, bool hasLeadingWhitespace, CompoundString* _Nonnull str);
extern Arithmetic* _Nonnull Arithmetic_CreateBinary(StackAllocatorRef _Nonnull pAllocator, bool hasLeadingWhitespace, ArithmeticType type, Arithmetic* _Nonnull lhs, Arithmetic* _Nonnull rhs);
extern Arithmetic* _Nonnull Arithmetic_CreateUnary(StackAllocatorRef _Nonnull pAllocator, bool hasLeadingWhitespace, ArithmeticType type, Arithmetic* _Nullable expr);
extern Arithmetic* _Nonnull Arithmetic_CreateVarRef(StackAllocatorRef _Nonnull pAllocator, bool hasLeadingWhitespace, VarRef* _Nonnull vref);
extern Arithmetic* _Nonnull Arithmetic_CreateIfThen(StackAllocatorRef _Nonnull pAllocator, bool hasLeadingWhitespace, Arithmetic* _Nonnull cond, struct Block* _Nonnull thenBlock, struct Block* _Nullable elseBlock);
extern Arithmetic* _Nonnull Arithmetic_CreateWhile(StackAllocatorRef _Nonnull pAllocator, bool hasLeadingWhitespace, Arithmetic* _Nonnull cond, struct Block* _Nonnull body);
extern Arithmetic* _Nonnull Arithmetic_CreateCommand(StackAllocatorRef _Nonnull pAllocator);
extern void CommandArithmetic_AddAtom(CommandArithmetic* _Nonnull self, Atom* _Nonnull atom);
#ifdef SCRIPT_PRINTING
extern void Arithmetic_Print(Arithmetic* _Nonnull self);
#endif



typedef enum ExpressionType {
    kExpression_Null,                   // Expression
    kExpression_ArithmeticExpression,   // ArithmeticExpression
    kExpression_Assignment,             // AssignmentExpression
    kExpression_VarDecl,                // VarDeclExpression
    kExpression_Break,                  // BreakExpression
    kExpression_Continue,               // ContinueExpression
} ExpressionType;

typedef struct Expression {
    struct Expression* _Nullable    next;
    int8_t                          type;
    bool                            isAsync;    // '&' -> true and ';' | '\n' -> false
} Expression;

typedef struct ArithmeticExpression {
    Expression              super;
    Arithmetic* _Nonnull    expr;
} ArithmeticExpression;

typedef struct AssignmentExpression {
    Expression              super;
    Arithmetic* _Nonnull    lvalue;
    Arithmetic* _Nonnull    rvalue;
} AssignmentExpression;

typedef struct VarDeclExpression {
    Expression              super;
    VarRef* _Nonnull        vref;
    Arithmetic* _Nonnull    expr;
    unsigned int            modifiers;
} VarDeclExpression;

typedef struct BreakExpression {
    Expression              super;
    Arithmetic* _Nullable   expr;
} BreakExpression;

typedef struct ContinueExpression {
    Expression  super;
} ContinueExpression;

extern Expression* _Nonnull Expression_CreateNull(StackAllocatorRef _Nonnull pAllocator);
extern Expression* _Nonnull Expression_CreateArithmeticExpression(StackAllocatorRef _Nonnull pAllocator, Arithmetic* _Nonnull expr);
extern Expression* _Nonnull Expression_CreateAssignment(StackAllocatorRef _Nonnull pAllocator, Arithmetic* _Nonnull lvalue, Arithmetic* _Nonnull rvalue);
extern Expression* _Nonnull Expression_CreateVarDecl(StackAllocatorRef _Nonnull pAllocator, unsigned int modifiers, VarRef* _Nonnull vref, struct Arithmetic* _Nonnull expr);
extern Expression* _Nonnull Expression_CreateBreak(StackAllocatorRef _Nonnull pAllocator, Arithmetic* _Nullable expr);
extern Expression* _Nonnull Expression_CreateContinue(StackAllocatorRef _Nonnull pAllocator);
#ifdef SCRIPT_PRINTING
extern void Expression_Print(Expression* _Nonnull self);
#endif



typedef struct ExpressionList {
    Expression* _Nullable   exprs;
    Expression* _Nullable   lastExpr;
} ExpressionList;

extern void ExpressionList_Init(ExpressionList* _Nonnull self);
extern void ExpressionList_AddExpression(ExpressionList* _Nonnull self, Expression* _Nonnull expr);
#ifdef SCRIPT_PRINTING
extern void ExpressionList_Print(ExpressionList* _Nonnull self);
#endif



typedef struct Block {
    ExpressionList  exprs;
} Block;

extern Block* _Nonnull Block_Create(StackAllocatorRef _Nonnull pAllocator);
#ifdef SCRIPT_PRINTING
extern void Block_Print(Block* _Nonnull self);
#endif



typedef struct Script {
    ExpressionList              exprs;
    ConstantsPool* _Nonnull     constantsPool;
    StackAllocatorRef _Nonnull  allocator;
} Script;

// The script manages a stack allocator which is used to store all nodes in the
// AST that the script manages.
extern Script* _Nonnull Script_Create(void);
extern void Script_Destroy(Script* _Nullable self);
extern void Script_Reset(Script* _Nonnull self);
#ifdef SCRIPT_PRINTING
extern void Script_Print(Script* _Nonnull self);
#endif

#endif  /* Script_h */
