//
//  Parser.c
//  sh
//
//  Created by Dietmar Planitzer on 1/2/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Parser.h"
#include <builtins/cmdlib.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define peek(__id) (Lexer_GetToken(&self->lexer)->id == (__id))
#define consume() Lexer_ConsumeToken(&self->lexer)

#define match(__id) Parser_Match(self, __id)


static bool Parser_IsPExpressionTerminator(Parser* _Nonnull self);
static errno_t Parser_PExpression(Parser* _Nonnull self, PExpression* _Nullable * _Nonnull _Nonnull pOutExpr);


errno_t Parser_Create(Parser* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Parser* self;
    
    try_null(self, calloc(1, sizeof(Parser)), errno);
    try(Lexer_Init(&self->lexer));

    *pOutSelf = self;
    return EOK;

catch:
    Parser_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void Parser_Destroy(Parser* _Nullable self)
{
    if (self) {
        Lexer_Deinit(&self->lexer);
        self->allocator = NULL;

        free(self);
    }
}

static errno_t Parser_Match(Parser* _Nonnull self, char id)
{
    const Token* t = Lexer_GetToken(&self->lexer);

    if (t->id == id) {
        Lexer_ConsumeToken(&self->lexer);
        return EOK;
    }
    else {
        return ESYNTAX;
    }
}

// sub-p-expr:  '(' p-expr [p-expr-terminator] ')'
static errno_t Parser_SubPExpression(Parser* _Nonnull self, PExpression* _Nullable * _Nonnull pOutExpr)
{
    decl_try_err();
    PExpression* p_expr = NULL;

    try(match('('));
    try(Parser_PExpression(self, &p_expr));
    if (Parser_IsPExpressionTerminator(self)) {
        p_expr->terminator = Lexer_GetToken(&self->lexer)->id;
    }
    else {
        p_expr->terminator = ';';
    }
    try(match(')'));

    *pOutExpr = p_expr;
    return EOK;

catch:
    *pOutExpr = NULL;
    return err;
}

static int GetAtomTypeFromToken(TokenId id)
{
    switch (id) {
        case kToken_UnquotedString:     return kAtom_UnquotedString;
        case kToken_SingleQuotedString: return kAtom_SingleQuotedString;
        case kToken_DoubleQuotedString: return kAtom_DoubleQuotedString;
        case kToken_EscapedCharacter:   return kAtom_EscapedCharacter;
        case kToken_VariableName:       return kAtom_VariableReference;
        case kToken_LessEqual:          return kAtom_LessEqual;
        case kToken_GreaterEqual:       return kAtom_GreaterEqual;
        case kToken_NotEqual:           return kAtom_NotEqual;
        case kToken_Equal:              return kAtom_Equal;
        case '<':                       return kAtom_Less;
        case '>':                       return kAtom_Greater;
        case '+':                       return kAtom_Plus;
        case '-':                       return kAtom_Minus;
        case '*':                       return kAtom_Multiply;
        case '/':                       return kAtom_Divide;
        case '=':                       return kAtom_Assignment;
        default: return -1;
    }
}

// s-expr: (UNQUOTED_STRING
//          | SINGLE_QUOTED_STRING 
//          | DOUBLE_QUOTED_STRING 
//          | ESCAPED_CHARACTER
//          | VARIABLE_REFERENCE 
//          | sub-p-expr
//          | <
//          | >
//          | >=
//          | <=
//          | ==
//          | !=
//          | +
//          | -
//          | *
//          | /
//          | =
//          | CHARACTER
//       )+
static errno_t Parser_SExpression(Parser* _Nonnull self, SExpression* _Nullable * _Nonnull pOutExpr)
{
    decl_try_err();
    SExpression* s_expr = NULL;
    PExpression* p_expr = NULL;
    Atom* atom = NULL;
    bool done = false;

    try(SExpression_Create(self->allocator, &s_expr));

    while (!done) {
        const Token* t = Lexer_GetToken(&self->lexer);

        switch (t->id) {
            case kToken_UnquotedString:
            case kToken_SingleQuotedString:
            case kToken_DoubleQuotedString:
            case kToken_EscapedCharacter:
            case kToken_VariableName:
                try(Atom_CreateWithString(self->allocator, GetAtomTypeFromToken(t->id), t->u.string, t->length, t->hasLeadingWhitespace, &atom));
                consume();
                break;

            case kToken_LessEqual:
            case kToken_GreaterEqual:
            case kToken_NotEqual:
            case kToken_Equal:
                try(Atom_Create(self->allocator, GetAtomTypeFromToken(t->id), t->hasLeadingWhitespace, &atom));
                consume();
                break;

            case kToken_Character:
                try(Atom_CreateWithCharacter(self->allocator, t->u.character, t->hasLeadingWhitespace, &atom));
                consume();
                break;

            case '<':
            case '>':
            case '+':
            case '-':
            case '*':
            case '/':
            case '=':
                try(Atom_Create(self->allocator, GetAtomTypeFromToken(t->id), t->hasLeadingWhitespace, &atom));
                consume();
                break;

            case '(':
                try(Parser_SubPExpression(self, &p_expr));
                try(Atom_CreateWithPExpression(self->allocator, p_expr, &atom));
                break;

            default:
                done = true;
                break;
        }

        if (s_expr) {
            SExpression_AddAtom(s_expr, atom);
            atom = NULL;
        }
    }

    *pOutExpr = s_expr;
    return EOK;

catch:
    *pOutExpr = NULL;
    return err;
}

// p-expr-terminator: '\n' | ';' | '&'
static bool Parser_IsPExpressionTerminator(Parser* _Nonnull self)
{
    switch (Lexer_GetToken(&self->lexer)->id) {
        case kToken_Newline:
        case kToken_Semicolon:
        case kToken_Ampersand:
            return true;

        default:
            return false;
    }
}

// p-expr:	s-expr ('|' s-expr)*
static errno_t Parser_PExpression(Parser* _Nonnull self, PExpression* _Nullable * _Nonnull pOutExpr)
{
    decl_try_err();
    PExpression* p_expr = NULL;
    SExpression* s_expr = NULL;

    try(PExpression_Create(self->allocator, &p_expr));

    try(Parser_SExpression(self, &s_expr));
    PExpression_AddSExpression(p_expr, s_expr);
    s_expr = NULL;

    while (peek('|')) {
        consume();

        try(Parser_SExpression(self, &s_expr));
        PExpression_AddSExpression(p_expr, s_expr);
        s_expr = NULL;
    }

    *pOutExpr = p_expr;
    return EOK;

catch:
    *pOutExpr = NULL;
    return err;
}

// block:	'{' (p-expr p-expr-terminator)* '}'
static errno_t Parser_Block(Parser* _Nonnull self, Block* _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();
    Block* block = NULL;
    PExpression* expr = NULL;

    try(Block_Create(self->allocator, &block));

    try(match('{'));
    for (;;) {
        if (peek('}')) {
            break;
        }

        try(Parser_PExpression(self, &expr));

        if (Parser_IsPExpressionTerminator(self)) {
            expr->terminator = Lexer_GetToken(&self->lexer)->id;
            consume();
        }
        else if (peek('}')) {
            expr->terminator = ';';
        }
        else {
            throw(ESYNTAX);
        }

        Block_AddPExpression(block, expr);
    }
    try(match('}'));

    *pOutBlock = block;
    return EOK;

catch:
    *pOutBlock = NULL;
    return err;
}

// script:	(p-expr p-expr-terminator)* EOF
static errno_t Parser_Script(Parser* _Nonnull self, Script* _Nonnull script)
{
    decl_try_err();
    Block* body = NULL;
    PExpression* expr = NULL;

    try(Block_Create(self->allocator, &body));
    Script_SetBlock(script, body);

    for (;;) {
        if (peek(kToken_Eof)) {
            break;
        }

        try(Parser_PExpression(self, &expr));
        
        if (Parser_IsPExpressionTerminator(self)) {
            expr->terminator = Lexer_GetToken(&self->lexer)->id;
            consume();
        }
        else if (peek(kToken_Eof)) {
            expr->terminator = ';';
        }
        else {
            throw(ESYNTAX);
        }

        Block_AddPExpression(body, expr);
    }

catch:
    return err;
}

// Parses the text 'text' and updates the script object 'script' to reflect the
// result of parsing 'text'.
errno_t Parser_Parse(Parser* _Nonnull self, const char* _Nonnull text, Script* _Nonnull script)
{
    decl_try_err();

    Lexer_SetInput(&self->lexer, text);
    self->allocator = script->allocator;
    err = Parser_Script(self, script);
    self->allocator = NULL;
    Lexer_SetInput(&self->lexer, NULL);

    return err;
}
