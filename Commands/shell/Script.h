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
#include "StackAllocator.h"

//#define SCRIPT_PRINTING 1
struct PExpression;


typedef enum AtomType {
    kAtom_Character,                // u.character  (a character that isn't recognized as an operator symbol)
    kAtom_UnquotedString,           // u.string
    kAtom_SingleQuotedString,       // u.string
    kAtom_DoubleQuotedString,       // u.string
    kAtom_EscapedCharacter,         // u.string
    kAtom_VariableReference,        // u.string
    kAtom_PExpression,              // u.expr
    kAtom_Less,
    kAtom_Greater,
    kAtom_LessEqual,
    kAtom_GreaterEqual,
    kAtom_NotEqual,
    kAtom_Equal,
    kAtom_Plus,
    kAtom_Minus,
    kAtom_Multiply,
    kAtom_Divide,
    kAtom_Assignment,
} AtomType;

struct AtomString {
    char*   chars;
    size_t  length;
};

typedef struct Atom {
    struct Atom* _Nullable  next;
    int8_t                  type;
    int8_t                  reserved[2];
    bool                    hasLeadingWhitespace;
    union {
        char                    character;
        struct AtomString       string;
        struct PExpression*     expr;
    }                       u;
} Atom;

extern errno_t Atom_Create(StackAllocatorRef _Nonnull pAllocator, AtomType type, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf);
extern errno_t Atom_CreateWithCharacter(StackAllocatorRef _Nonnull pAllocator, char ch, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf);
extern errno_t Atom_CreateWithString(StackAllocatorRef _Nonnull pAllocator, AtomType type, const char* _Nonnull str, size_t len, bool hasLeadingWhitespace, Atom* _Nullable * _Nonnull pOutSelf);
extern errno_t Atom_CreateWithPExpression(StackAllocatorRef _Nonnull pAllocator, struct PExpression* _Nonnull expr, Atom* _Nullable * _Nonnull pOutSelf);  // Takes ownership of the p-expression
#ifdef SCRIPT_PRINTING
extern void Atom_Print(Atom* _Nonnull self);
#endif


typedef struct SExpression {
    struct SExpression* _Nullable   next;
    Atom* _Nonnull                  atoms;
    Atom* _Nonnull                  lastAtom;
} SExpression;

extern errno_t SExpression_Create(StackAllocatorRef _Nonnull pAllocator, SExpression* _Nullable * _Nonnull pOutSelf);
extern void SExpression_AddAtom(SExpression* _Nonnull self, Atom* _Nonnull atom);
#ifdef SCRIPT_PRINTING
extern void SExpression_Print(SExpression* _Nonnull self);
#endif



typedef struct PExpression {
    struct PExpression* _Nullable   next;
    SExpression* _Nonnull           exprs;
    SExpression* _Nonnull           lastExpr;
    TokenId                         terminator;
} PExpression;

extern errno_t PExpression_Create(StackAllocatorRef _Nonnull pAllocator, PExpression* _Nullable * _Nonnull pOutSelf);
extern void PExpression_AddSExpression(PExpression* _Nonnull self, SExpression* _Nonnull expr);
#ifdef SCRIPT_PRINTING
extern void PExpression_Print(PExpression* _Nonnull self);
#endif



typedef struct Block {
    PExpression* _Nullable  exprs;
    PExpression* _Nullable  lastExpr;
} Block;

extern errno_t Block_Create(StackAllocatorRef _Nonnull pAllocator, Block* _Nullable * _Nonnull pOutSelf);
extern void Block_AddPExpression(Block* _Nonnull self, PExpression* _Nonnull expr);
#ifdef SCRIPT_PRINTING
extern void Block_Print(Block* _Nonnull self);
#endif



typedef struct Script {
    Block* _Nullable            body;
    StackAllocatorRef _Nonnull  allocator;
} Script;

// The script manages a stack allocator which is used to store all nodes in the
// AST that the script manages.
extern errno_t Script_Create(Script* _Nullable * _Nonnull pOutSelf);
extern void Script_Destroy(Script* _Nullable self);
extern void Script_Reset(Script* _Nonnull self);
extern void Script_SetBlock(Script* _Nonnull self, Block* _Nonnull block);
#ifdef SCRIPT_PRINTING
extern void Script_Print(Script* _Nonnull self);
#endif

#endif  /* Script_h */
