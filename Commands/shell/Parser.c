//
//  Parser.c
//  sh
//
//  Created by Dietmar Planitzer on 1/2/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Parser.h"
#include "Utilities.h"
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define peek(__id) (Lexer_GetToken(&self->lexer)->id == (__id))
#define consume() Lexer_ConsumeToken(&self->lexer)

#define match(__id) Parser_Match(self, __id)
#define matchAndSwitchMode(__id, __mode) Parser_MatchAndSwitchMode(self, __id, __mode)
#define failOnIncomplete(__t) ((__t)->isIncomplete) ? ESYNTAX : EOK

static errno_t Parser_VarReference(Parser* _Nonnull self, VarRef* _Nullable * _Nonnull pOutVarRef);
static errno_t Parser_EscapedArithmeticExpression(Parser* _Nonnull self, Arithmetic* _Nullable * _Nonnull pOutExpr);
static errno_t Parser_ParenthesizedArithmeticExpression(Parser* _Nonnull self, Arithmetic* _Nullable * _Nonnull pOutExpr);
static errno_t Parser_ArithmeticExpression(Parser* _Nonnull self, Arithmetic* _Nullable * _Nonnull _Nonnull pOutExpr);
static errno_t Parser_Block(Parser* _Nonnull self, Block* _Nullable * _Nonnull pOutBlock);


errno_t Parser_Create(Parser* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Parser* self;
    
    try_null(self, calloc(1, sizeof(Parser)), errno);
    Lexer_Init(&self->lexer);

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
        self->constantsPool = NULL;

        free(self);
    }
}

static errno_t Parser_Match(Parser* _Nonnull self, TokenId id)
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

static errno_t Parser_MatchAndSwitchMode(Parser* _Nonnull self, TokenId id, LexerMode mode)
{
    if (Lexer_GetToken(&self->lexer)->id != id) {
        return ESYNTAX;
    }
    Lexer_SetMode(&self->lexer, mode);
    Lexer_ConsumeToken(&self->lexer);
    return EOK;
}


//
// doubleQuotedString
//     : DOUBLE_QUOTE 
//         ( STRING_SEGMENT(dq_mode)
//         | ESCAPE_SEQUENCE(dq_mode)
//         | escapedArithmeticExpression(dq_mode)
//         | VAR_NAME(dq_mode)
//       )* DOUBLE_QUOTE(dq_mode)
//     ;
static errno_t Parser_CompoundString(Parser* _Nonnull self, bool isBacktick, CompoundString* _Nullable * _Nonnull pOutStr)
{
    decl_try_err();
    CompoundString* str = NULL;
    Segment* seg = NULL;
    TokenId quoteToken = (isBacktick) ? kToken_DoubleBacktick : kToken_DoubleQuote;
    LexerMode lexerMode = (isBacktick) ? kLexerMode_DoubleBacktick : kLexerMode_DoubleQuote;
    Value v;
    bool done = false;

    try(matchAndSwitchMode(quoteToken, lexerMode));
    try(CompoundString_Create(self->allocator, &str));

    while(!done) {
        const Token* t = Lexer_GetToken(&self->lexer);

        switch (t->id) {
            case kToken_StringSegment:
                try(ConstantsPool_GetStringValue(self->constantsPool, t->u.string, t->length, &v));
                try(Segment_CreateLiteral(self->allocator, kSegment_String, &v, &seg));
                consume();
                break;

            case kToken_EscapeSequence:
                try(failOnIncomplete(t));
                try(ConstantsPool_GetStringValue(self->constantsPool, t->u.string, t->length, &v));
                try(Segment_CreateLiteral(self->allocator, kSegment_EscapeSequence, &v, &seg));
                consume();
                break;

            case kToken_EscapedExpression: {
                Arithmetic* expr = NULL;
                try(Parser_EscapedArithmeticExpression(self, &expr));
                try(Segment_CreateArithmeticExpression(self->allocator, expr, &seg));
                break;
            }

            case kToken_VariableName: {
                VarRef* vref = NULL;
                try(Parser_VarReference(self, &vref));
                try(Segment_CreateVarRef(self->allocator, vref, &seg));
                break;
            }

            default:
                if (t->id == quoteToken) {
                    done = true;
                    break;
                }
                throw(ESYNTAX);
                break;
        }

        if (seg) {
            CompoundString_AddSegment(str, seg);
            seg = NULL;
        }
    }
    Lexer_SetMode(&self->lexer, kLexerMode_Default);
    consume();

    *pOutStr = str;
    return EOK;

catch:
    Lexer_SetMode(&self->lexer, kLexerMode_Default);
    *pOutStr = NULL;
    return err;
}

//
// escapedArithmeticExpression
//     : ESCAPED_EXPRESSION arithmeticExpression CLOSE_PARA
//     ;
static errno_t Parser_EscapedArithmeticExpression(Parser* _Nonnull self, Arithmetic* _Nullable * _Nonnull pOutExpr)
{
    decl_try_err();
    LexerMode savedMode = Lexer_GetMode(&self->lexer);
    Arithmetic* expr = NULL;

    try(matchAndSwitchMode(kToken_EscapedExpression, kLexerMode_Default));
    try(Parser_ArithmeticExpression(self, &expr));
    try(matchAndSwitchMode(kToken_ClosingParenthesis, savedMode));

    *pOutExpr = expr;
    return EOK;

catch:
    Lexer_SetMode(&self->lexer, savedMode);
    *pOutExpr = NULL;
    return err;
}

//
// parenthesizedArithmeticExpression
//     : OPEN_PARA arithmeticExpression CLOSE_PARA
//     ;
static errno_t Parser_ParenthesizedArithmeticExpression(Parser* _Nonnull self, Arithmetic* _Nullable * _Nonnull pOutExpr)
{
    decl_try_err();
    Arithmetic* expr = NULL;

    try(match(kToken_OpeningParenthesis));
    try(Parser_ArithmeticExpression(self, &expr));
    try(match(kToken_ClosingParenthesis));

    *pOutExpr = expr;
    return EOK;

catch:
    *pOutExpr = NULL;
    return err;
}

//
// varReference
//     : VAR_NAME
//     ;
static errno_t Parser_VarReference(Parser* _Nonnull self, VarRef* _Nullable * _Nonnull pOutVarRef)
{
    decl_try_err();
    const Token* t = Lexer_GetToken(&self->lexer);

    if (t->id != kToken_VariableName) {
        throw(ESYNTAX);
    }

    try(VarRef_Create(self->allocator, t->u.string, pOutVarRef));
    consume();
    return EOK;

catch:
    *pOutVarRef = NULL;
    return err;
}

static const char* _Nonnull get_op2_token_string(TokenId id)
{
    switch (id) {
        case kToken_Conjunction:        return "&&";
        case kToken_Disjunction:        return "||";
        case kToken_EqualEqual:         return "==";
        case kToken_GreaterEqual:       return ">=";
        case kToken_LessEqual:          return "<=";
        case kToken_NotEqual:           return "!=";
        default:                        abort(); break;
    }
}

//
// commandPrimaryFragment
//     : SINGLE_BACKTICK_STRING
//         | SLASH
//         | IDENTIFIER
//         | doubleBacktickString
//         ;
//     ;
static errno_t Parser_CommandPrimaryFragment(Parser* _Nonnull self, Atom* _Nullable * _Nonnull pOutAtom)
{
    decl_try_err();
    const Token* t = Lexer_GetToken(&self->lexer);
    const bool hasLeadingWhitespace = t->hasLeadingWhitespace;

    *pOutAtom = NULL;

    switch (t->id) {
        case kToken_BacktickString:
            try(failOnIncomplete(t));
            try(Atom_CreateWithString(self->allocator, kAtom_BacktickString, t->u.string, t->length, hasLeadingWhitespace, pOutAtom));
            consume();
            break;

        case kToken_DoubleBacktick: {
            CompoundString* str = NULL;
            try(Parser_CompoundString(self, true, &str));
            try(Atom_CreateWithCompoundString(self->allocator, kAtom_DoubleBacktickString, str, hasLeadingWhitespace, pOutAtom));
            break;
        }

        case kToken_Slash:
            try(Atom_CreateWithCharacter(self->allocator, kAtom_Identifier, t->id, hasLeadingWhitespace, pOutAtom));
            consume();
            break;

        case kToken_Identifier:
            try(failOnIncomplete(t));
            try(Atom_CreateWithString(self->allocator, kAtom_Identifier, t->u.string, t->length, hasLeadingWhitespace, pOutAtom));
            consume();
            break;

        default:
            break;
    }

catch:
    return err;
}

//
// commandSecondaryFragment
//     : IDENTIFIER
//     | ELSE
//     | FALSE
//     | IF
//     | INTERNAL
//     | LET
//     | VAR
//     | WHILE
//     | PUBLIC
//     | TRUE
//     | ASSIGNMENT
//     | CONJUNCTION
//     | DISJUNCTION
//     | PERCENT
//     | PLUS
//     | MINUS
//     | ASTERISK
//     | SLASH
//     | EQEQ
//     | NOEQ
//     | LEEQ
//     | GREQ
//     | LESS
//     | GREATER
//     | VAR_NAME
//     | SINGLE_BACKTICK_STRING
//     | doubleBacktickString
//     | literal
//     | parenthesizedArithmeticExpression
//     ;
static errno_t Parser_CommandSecondaryFragment(Parser* _Nonnull self, Atom* _Nullable * _Nonnull pOutAtom)
{
    decl_try_err();
    const Token* t = Lexer_GetToken(&self->lexer);
    const bool hasLeadingWhitespace = t->hasLeadingWhitespace;

    *pOutAtom = NULL;

    switch (t->id) {
        case kToken_Assignment:
        case kToken_Asterisk:
        case kToken_Bang:
        case kToken_Greater:
        case kToken_Less:
        case kToken_Minus:
        case kToken_Percent:
        case kToken_Plus:
        case kToken_Slash:
            try(Atom_CreateWithCharacter(self->allocator, kAtom_Identifier, t->id, hasLeadingWhitespace, pOutAtom));
            consume();
            break;

        case kToken_Conjunction:
        case kToken_Disjunction:
        case kToken_EqualEqual:
        case kToken_GreaterEqual:
        case kToken_LessEqual:
        case kToken_NotEqual:
            try(Atom_CreateWithString(self->allocator, kAtom_Identifier, get_op2_token_string(t->id), 2, hasLeadingWhitespace, pOutAtom));
            consume();
            break;

        case kToken_BacktickString:
            try(failOnIncomplete(t));
            try(Atom_CreateWithString(self->allocator, kAtom_BacktickString, t->u.string, t->length, hasLeadingWhitespace, pOutAtom));
            consume();
            break;

        case kToken_DoubleBacktick: {
            CompoundString* str = NULL;
            try(Parser_CompoundString(self, true, &str));
            try(Atom_CreateWithCompoundString(self->allocator, kAtom_DoubleBacktickString, str, hasLeadingWhitespace, pOutAtom));
            break;
        }

        case kToken_SingleQuoteString:
            try(failOnIncomplete(t));
            try(Atom_CreateWithString(self->allocator, kAtom_SingleQuoteString, t->u.string, t->length, hasLeadingWhitespace, pOutAtom));
            consume();
            break;

        case kToken_DoubleQuote: {
            CompoundString* str = NULL;
            try(Parser_CompoundString(self, t->id == kToken_DoubleBacktick, &str));
            try(Atom_CreateWithCompoundString(self->allocator, kAtom_DoubleQuoteString, str, hasLeadingWhitespace, pOutAtom));
            break;
        }

        case kToken_Integer:
            try(Atom_CreateWithInteger(self->allocator, t->u.i32, hasLeadingWhitespace, pOutAtom));
            consume();
            break;

        case kToken_Identifier:
        case kToken_Else:
        case kToken_False:
        case kToken_If:
        case kToken_Internal:
        case kToken_Let:
        case kToken_Public:
        case kToken_True:
        case kToken_Var:
        case kToken_While:
            try(failOnIncomplete(t));
            try(Atom_CreateWithString(self->allocator, kAtom_Identifier, t->u.string, t->length, hasLeadingWhitespace, pOutAtom));
            consume();
            break;

        case kToken_OpeningParenthesis: {
            Arithmetic* expr = NULL;
            try(Parser_ParenthesizedArithmeticExpression(self, &expr));
            try(Atom_CreateWithArithmeticExpression(self->allocator, expr, hasLeadingWhitespace, pOutAtom));
            break;
        }

        case kToken_VariableName: {
            VarRef* vref = NULL;
            try(Parser_VarReference(self, &vref));
            try(Atom_CreateWithVarRef(self->allocator, vref, hasLeadingWhitespace, pOutAtom));
            break;
        }

        default:
            break;
    }

catch:
    return err;
}

//
// command
//     : commandPrimaryFragment commandSecondaryFragment*
//     ;
static errno_t Parser_Command(Parser* _Nonnull self, Arithmetic* _Nullable * _Nonnull pOutCmd)
{
    decl_try_err();
    Arithmetic* expr = NULL;
    Atom* atom = NULL;

    try(Arithmetic_CreateCommand(self->allocator, &expr));
    CommandArithmetic* cmd = AS(expr, CommandArithmetic);


    // Primary fragment is required
    try(Parser_CommandPrimaryFragment(self, &atom));
    if (atom == NULL) {
        throw(ESYNTAX);
    }
    CommandArithmetic_AddAtom(cmd, atom);


    // Secondary fragments are optional
    for (;;) {
        try(Parser_CommandSecondaryFragment(self, &atom));
        if (atom == NULL) {
            break;
        }

        CommandArithmetic_AddAtom(cmd, atom);
        atom = NULL;
    }

    *pOutCmd = expr;
    return EOK;

catch:
    *pOutCmd = NULL;
    return err;
}

//
// literal
//     : FALSE
//     | TRUE
//     | INTEGER
//     | SINGLE_QUOTED_STRING
//     | doubleQuotedString
//     ;
static errno_t Parser_Literal(Parser* _Nonnull self, Arithmetic* _Nullable * _Nonnull pOutExpr)
{
    decl_try_err();
    const Token* t = Lexer_GetToken(&self->lexer);
    const bool hasLeadingWhitespace = t->hasLeadingWhitespace;
    Arithmetic* expr = NULL;
    Value v;

    switch (t->id) {
        case kToken_False:
            Value_InitBool(&v, false);
            try(Arithmetic_CreateLiteral(self->allocator, hasLeadingWhitespace, &v, pOutExpr));
            consume();
            break;

        case kToken_True:
            Value_InitBool(&v, true);
            try(Arithmetic_CreateLiteral(self->allocator, hasLeadingWhitespace, &v, pOutExpr));
            consume();
            break;
            
        case kToken_Integer:
            Value_InitInteger(&v, t->u.i32);
            try(Arithmetic_CreateLiteral(self->allocator, hasLeadingWhitespace, &v, pOutExpr));
            consume();
            break;

        case kToken_SingleQuoteString:
            try(ConstantsPool_GetStringValue(self->constantsPool, t->u.string, t->length, &v));
            try(Arithmetic_CreateLiteral(self->allocator, hasLeadingWhitespace, &v, pOutExpr));
            consume();
            break;

        case kToken_DoubleQuote: {
            CompoundString* str = NULL;
            try(Parser_CompoundString(self, false, &str));
            try(Arithmetic_CreateCompoundString(self->allocator, hasLeadingWhitespace, str, pOutExpr));
            break;
        }

        default:
            throw(ESYNTAX);
            break;
    }
    return EOK;

catch:
    *pOutExpr = NULL;
    return err;
}

//
// ifExpression
//    : IF arithmeticExpression block (ELSE block)?
//    ;
static errno_t Parser_IfExpression(Parser* _Nonnull self, Arithmetic* _Nullable * _Nonnull pOutExpr)
{
    decl_try_err();
    Arithmetic* condExpr = NULL;
    Block* thenBlock = NULL;
    Block* elseBlock = NULL;

    try(match(kToken_If));
    const bool hasLeadingWhitespace = Lexer_GetToken(&self->lexer)->hasLeadingWhitespace;

    try(Parser_ArithmeticExpression(self, &condExpr));
    try(Parser_Block(self, &thenBlock));
    if (peek(kToken_Else)) {
        consume();
        try(Parser_Block(self, &elseBlock));
    }

    return Arithmetic_CreateIfThen(self->allocator, hasLeadingWhitespace, condExpr, thenBlock, elseBlock, pOutExpr);

catch:
    *pOutExpr = NULL;
    return err;
}

//
// whileExpression
//     : WHILE arithmeticExpression block
//     ;
static errno_t Parser_WhileExpression(Parser* _Nonnull self, Arithmetic* _Nullable * _Nonnull pOutExpr)
{
    decl_try_err();
    Arithmetic* condExpr = NULL;
    Block* bodyBlock = NULL;

    try(match(kToken_While));
    const bool hasLeadingWhitespace = Lexer_GetToken(&self->lexer)->hasLeadingWhitespace;

    try(Parser_ArithmeticExpression(self, &condExpr));
    try(Parser_Block(self, &bodyBlock));

    return Arithmetic_CreateWhile(self->allocator, hasLeadingWhitespace, condExpr, bodyBlock, pOutExpr);

catch:
    *pOutExpr = NULL;
    return err;
}

//
// primaryExpression
//     : literal
//     | VAR_NAME
//     | parenthesizedArithmeticExpression
//     | conditionalExpression
//     | loopExpression
//    ;
static errno_t Parser_PrimaryExpression(Parser* _Nonnull self, Arithmetic* _Nullable * _Nonnull pOutExpr)
{
    decl_try_err();
    const Token* t = Lexer_GetToken(&self->lexer);
    const bool hasLeadingWhitespace = t->hasLeadingWhitespace;
    Arithmetic* expr = NULL;

    switch (t->id) {
        case kToken_VariableName: {
            VarRef* vref = NULL;
            try(Parser_VarReference(self, &vref));
            try(Arithmetic_CreateVarRef(self->allocator, hasLeadingWhitespace, vref, pOutExpr));
            break;
        }

        case kToken_OpeningParenthesis: {
            Arithmetic* expr = NULL;
            try(Parser_ParenthesizedArithmeticExpression(self, &expr));
            try(Arithmetic_CreateUnary(self->allocator, hasLeadingWhitespace, kArithmetic_Parenthesized, expr, pOutExpr));
            break;
        }

        case kToken_If:
            try(Parser_IfExpression(self, pOutExpr));
            break;

        case kToken_While:
            try(Parser_WhileExpression(self, pOutExpr));
            break;

        default:
            try(Parser_Literal(self, pOutExpr));
            break;
    }
    return EOK;

catch:
    *pOutExpr = NULL;
    return err;
}

//
// prefixUnaryExpression
//     : prefixOperator* primaryExpression
//     ;
static errno_t Parser_PrefixUnaryExpression(Parser* _Nonnull self, Arithmetic* _Nullable * _Nonnull pOutExpr)
{
    decl_try_err();
    Arithmetic* firstExpr = NULL;
    Arithmetic* curExpr = NULL;
    Arithmetic* expr = NULL;

    // Grab all prefix operators
    for (;;) {
        const Token* t = Lexer_GetToken(&self->lexer);
        const bool hasLeadingWhitespace = t->hasLeadingWhitespace;
        bool done = false;
        ArithmeticType type;

        switch (t->id) {
            case kToken_Plus:   type = kArithmetic_Positive; break;
            case kToken_Minus:  type = kArithmetic_Negative; break;
            case kToken_Bang:   type = kArithmetic_Not; break;
            default:            done = true; break;
        }
        if (done) {
            break;
        }

        consume();
        try(Arithmetic_CreateUnary(self->allocator, hasLeadingWhitespace, type, NULL, &expr));
        if (curExpr) {
            AS(curExpr, UnaryArithmetic)->expr = expr;
            curExpr = expr;
        }
        else {
            firstExpr = expr;
            curExpr = expr;
        }
        expr = NULL;
    }


    // Grab the value the operators apply to
    try(Parser_PrimaryExpression(self, &expr));
    if (curExpr) {
        AS(curExpr, UnaryArithmetic)->expr = expr;
        *pOutExpr = firstExpr;
    }
    else {
        *pOutExpr = expr;
    }
    
    return EOK;

catch:
    *pOutExpr = NULL;
    return err;
}

//
// multiplication
//     : prefixUnaryExpression (multiplicationOperator prefixUnaryExpression)*
//     ;
static errno_t Parser_Multiplication(Parser* _Nonnull self, Arithmetic* _Nullable * _Nonnull pOutExpr)
{
    decl_try_err();
    Arithmetic* expr = NULL;
    Arithmetic* lhs = NULL;
    Arithmetic* rhs = NULL;

    try(Parser_PrefixUnaryExpression(self, &lhs));

    for (;;) {
        const Token* t = Lexer_GetToken(&self->lexer);
        const bool hasLeadingWhitespace = t->hasLeadingWhitespace;
        ArithmeticType type;
        bool done = false;

        switch (t->id) {
            case kToken_Asterisk:   type = kArithmetic_Multiplication; break;
            case kToken_Slash:      type = kArithmetic_Division; break;
            case kToken_Percent:    type = kArithmetic_Modulo; break;
            default:                done = true; break;
        }
        if (done) {
            break;
        }

        consume();
        try(Parser_PrefixUnaryExpression(self, &rhs));
        try(Arithmetic_CreateBinary(self->allocator, hasLeadingWhitespace, type, lhs, rhs, &expr));
        lhs = expr;
        expr = NULL;
    }

    *pOutExpr = lhs;
    return EOK;

catch:
    *pOutExpr = NULL;
    return err;
}

//
// addition
//     : multiplication (additionOperator multiplication)*
//     ;
static errno_t Parser_Addition(Parser* _Nonnull self, Arithmetic* _Nullable * _Nonnull pOutExpr)
{
    decl_try_err();
    Arithmetic* expr = NULL;
    Arithmetic* lhs = NULL;
    Arithmetic* rhs = NULL;

    try(Parser_Multiplication(self, &lhs));

    for (;;) {
        const Token* t = Lexer_GetToken(&self->lexer);
        const bool hasLeadingWhitespace = t->hasLeadingWhitespace;
        ArithmeticType type;
        bool done = false;

        switch (t->id) {
            case kToken_Plus:   type = kArithmetic_Addition; break;
            case kToken_Minus:  type = kArithmetic_Subtraction; break;
            default:            done = true; break;
        }
        if (done) {
            break;
        }

        consume();
        try(Parser_Multiplication(self, &rhs));
        try(Arithmetic_CreateBinary(self->allocator, hasLeadingWhitespace, type, lhs, rhs, &expr));
        lhs = expr;
        expr = NULL;
    }

    *pOutExpr = lhs;
    return EOK;

catch:
    *pOutExpr = NULL;
    return err;
}

//
// comparison
//     : addition (comparisonOperator addition)*
//     ;
static errno_t Parser_Comparison(Parser* _Nonnull self, Arithmetic* _Nullable * _Nonnull pOutExpr)
{
    decl_try_err();
    Arithmetic* expr = NULL;
    Arithmetic* lhs = NULL;
    Arithmetic* rhs = NULL;

    try(Parser_Addition(self, &lhs));

    for (;;) {
        const Token* t = Lexer_GetToken(&self->lexer);
        const bool hasLeadingWhitespace = t->hasLeadingWhitespace;
        ArithmeticType type;
        bool done = false;

        switch (t->id) {
            case kToken_EqualEqual:     type = kArithmetic_Equals; break;
            case kToken_NotEqual:       type = kArithmetic_NotEquals; break;
            case kToken_LessEqual:      type = kArithmetic_LessEquals; break;
            case kToken_GreaterEqual:   type = kArithmetic_GreaterEquals; break;
            case kToken_Less:           type = kArithmetic_Less; break;
            case kToken_Greater:        type = kArithmetic_Greater; break;
            default:                    done = true; break;
        }
        if (done) {
            break;
        }

        consume();
        try(Parser_Addition(self, &rhs));
        try(Arithmetic_CreateBinary(self->allocator, hasLeadingWhitespace, type, lhs, rhs, &expr));
        lhs = expr;
        expr = NULL;
    }

    *pOutExpr = lhs;
    return EOK;

catch:
    *pOutExpr = NULL;
    return err;
}

//
// conjunction
//     : comparison (CONJUNCTION comparison)*
//     ;
static errno_t Parser_Conjunction(Parser* _Nonnull self, Arithmetic* _Nullable * _Nonnull pOutExpr)
{
    decl_try_err();
    Arithmetic* expr = NULL;
    Arithmetic* lhs = NULL;
    Arithmetic* rhs = NULL;

    try(Parser_Comparison(self, &lhs));

    while (peek(kToken_Conjunction)) {
        const bool hasLeadingWhitespace = Lexer_GetToken(&self->lexer)->hasLeadingWhitespace;

        consume();
        try(Parser_Comparison(self, &rhs));
        try(Arithmetic_CreateBinary(self->allocator, hasLeadingWhitespace, kArithmetic_Conjunction, lhs, rhs, &expr));
        lhs = expr;
        expr = NULL;
    }

    *pOutExpr = lhs;
    return EOK;

catch:
    *pOutExpr = NULL;
    return err;
}

//
// disjunction
//     : conjunction (DISJUNCTION conjunction)*
//     ;
static errno_t Parser_Disjunction(Parser* _Nonnull self, Arithmetic* _Nullable * _Nonnull pOutExpr)
{
    decl_try_err();
    Arithmetic* expr = NULL;
    Arithmetic* lhs = NULL;
    Arithmetic* rhs = NULL;

    try(Parser_Conjunction(self, &lhs));

    while (peek(kToken_Disjunction)) {
        const bool hasLeadingWhitespace = Lexer_GetToken(&self->lexer)->hasLeadingWhitespace;

        consume();
        try(Parser_Conjunction(self, &rhs));
        try(Arithmetic_CreateBinary(self->allocator, hasLeadingWhitespace, kArithmetic_Disjunction, lhs, rhs, &expr));
        lhs = expr;
        expr = NULL;
    }

    *pOutExpr = lhs;
    return EOK;

catch:
    *pOutExpr = NULL;
    return err;
}

//
// arithmeticExpression
//     : (disjunction | command) (BAR command)*
//     ;
static errno_t Parser_ArithmeticExpression(Parser* _Nonnull self, Arithmetic* _Nullable * _Nonnull pOutExpr)
{
    decl_try_err();
    Arithmetic* expr = NULL;
    Arithmetic* lhs = NULL;
    Arithmetic* rhs = NULL;

    // The first component may be a mathematical expression or a command
    switch (Lexer_GetToken(&self->lexer)->id) {
        case kToken_BacktickString:
        case kToken_DoubleBacktick:
        case kToken_Slash:
        case kToken_Identifier:
            // a command
            try(Parser_Command(self, &lhs));
            break;

        default:
            // a disjunction
            try(Parser_Disjunction(self, &lhs));
            break;
    }

    // The rest is the tail of a pipeline if the rest exists 
    while (peek(kToken_Bar)) {
        const bool hasLeadingWhitespace = Lexer_GetToken(&self->lexer)->hasLeadingWhitespace;

        consume();
        try(Parser_Command(self, &rhs));
        try(Arithmetic_CreateBinary(self->allocator, hasLeadingWhitespace, kArithmetic_Pipeline, lhs, rhs, &expr));
        lhs = expr;
        expr = NULL;
    }

    *pOutExpr = lhs;
    return EOK;

catch:
    *pOutExpr = NULL;
    return err;
}

//
// varDeclExpression
//     : (INTERNAL | PUBLIC)? (LET | VAR) VAR_NAME ASSIGNMENT arithmeticExpression
//     ;
static errno_t Parser_VarDeclExpression(Parser* _Nonnull self, Expression* _Nullable * _Nonnull pOutDecl)
{
    decl_try_err();
    VarRef* vref = NULL;
    Arithmetic* expr = NULL;
    unsigned int modifiers = 0;

    if (match(kToken_Public) == EOK) {
        modifiers |= kVarModifier_Public;
    }
    else if (match(kToken_Internal) == EOK) {
        modifiers &= ~kVarModifier_Public;
    }

    if (match(kToken_Let) == EOK) {
        modifiers &= ~kVarModifier_Mutable;
    }
    else if (match(kToken_Var) == EOK) {
        modifiers |= kVarModifier_Mutable;
    }
    else {
        throw(ESYNTAX);
    }

    const Token* t = Lexer_GetToken(&self->lexer);
    if (t->id != kToken_VariableName) {
        throw(ESYNTAX);
    }
    try(VarRef_Create(self->allocator, t->u.string, &vref));
    consume();

    try(match(kToken_Assignment));
    try(Parser_ArithmeticExpression(self, &expr));

    try(Expression_CreateVarDecl(self->allocator, modifiers, vref, expr, pOutDecl));
    return EOK;

catch:
    *pOutDecl = NULL;
    return err;
}

//
// breakExpression
//     : BREAK arithmeticExpression?
//     ;
static errno_t Parser_BreakExpression(Parser* _Nonnull self, Expression* _Nullable * _Nonnull pOutExpr)
{
    decl_try_err();
    Expression* expr = NULL;
    Arithmetic* arithExpr = NULL;

    try(match(kToken_Break));

    switch (Lexer_GetToken(&self->lexer)->id) {
        case kToken_Newline:
        case kToken_Semicolon:
        case kToken_Ampersand:
        case kToken_ClosingBrace:
        case kToken_Eof:
            // This break has no arithmetic expression associated with it
            break;

        default:
            // Arithmetic expression
            try(Parser_ArithmeticExpression(self, &arithExpr));
            break;
    }

    try(Expression_CreateBreak(self->allocator, arithExpr, &expr));

    *pOutExpr = expr;
    return EOK;

catch:
    *pOutExpr = NULL;
    return err;
}

//
// terminator
//     : NL | SEMICOLON | AMPERSAND
//     ;
static errno_t Parser_Terminator(Parser* _Nonnull self, bool isNested, bool* _Nonnull pOutIsAsync)
{
    decl_try_err();
    const Token* t = Lexer_GetToken(&self->lexer);
    bool isAsync = false;

    switch (t->id) {
        case kToken_Newline:
        case kToken_Semicolon:
        case kToken_Ampersand:
            isAsync = (t->id == kToken_Ampersand) ? true : false;
            consume();

            // Optimization: we don't want to generate expressions for '<null>\n'
            while (peek(kToken_Newline)) {
                consume();
            }
            break;

        case kToken_ClosingBrace:
            if (isNested) {
                // Accept '}' as an expression terminator if this is a nested
                // expression (aka the expression is inside a block)
                break;
            }
            err = ESYNTAX;
            break;

        default:
            if (!isNested && t->id == kToken_Eof) {
                // Accept scripts where the last line is terminated by EOF
                // since this is what we get in interactive mode anyway
                break; 
            }
            err = ESYNTAX;
            break;
    }

    *pOutIsAsync = isAsync;
    return err;
}

//
// expression
//     : (varDeclExpression
//         | assignmentExpression
//         | arithmeticExpression
//         | breakExpression
//         | CONTINUE)? terminator
//     ;
//
// assignmentExpression
//     : assignableExpression ASSIGNMENT arithmeticExpression
//     ;
//
// assignableExpression
//     : VAR_NAME
//     ;
//
// breakExpression
//     : BREAK arithmeticExpression?
//     ;
static errno_t Parser_Expression(Parser* _Nonnull self, bool isNested, Expression* _Nullable * _Nonnull pOutExpr)
{
    decl_try_err();
    const Token* t = Lexer_GetToken(&self->lexer);
    Expression* expr = NULL;

    switch (t->id) {
        case kToken_Newline:
        case kToken_Semicolon:
        case kToken_Ampersand:
        case kToken_Eof:
            // Null expression
            try(Expression_CreateNull(self->allocator, &expr));
            break;

        case kToken_Public:
        case kToken_Internal:
        case kToken_Let:
        case kToken_Var:
            // Variable declaration
            try(Parser_VarDeclExpression(self, &expr));
            break;

        case kToken_Break:
            try(Parser_BreakExpression(self, &expr));
            break;

        case kToken_Continue:
            consume();
            try(Expression_CreateContinue(self->allocator, &expr));
            break;

        default: {
            // Arithmetic expression or assignment
            Arithmetic* arithExpr = NULL;
            try(Parser_ArithmeticExpression(self, &arithExpr));

            // Check for assignment expression
            if (peek(kToken_Assignment)) {
                Arithmetic* rvalue = NULL;

                consume();
                try(Parser_ArithmeticExpression(self, &rvalue));
                try(Expression_CreateAssignment(self->allocator, arithExpr, rvalue, &expr));
            }
            else {
                try(Expression_CreateArithmeticExpression(self->allocator, arithExpr, &expr));
            }
        }
    }

    // Required terminator
    try(Parser_Terminator(self, isNested, &expr->isAsync));

    *pOutExpr = expr;
    return EOK;

catch:
    *pOutExpr = NULL;
    return err;
}

//
// expressions:
//     : expression*
//     ;
static errno_t Parser_ExpressionList(Parser* _Nonnull self, ExpressionList* _Nonnull exprList, int endToken, bool isNested)
{
    decl_try_err();
    Expression* expr;

    while (!peek(endToken)) {
        err = Parser_Expression(self, isNested, &expr);
        if (err != EOK) {
            break;
        }

        ExpressionList_AddExpression(exprList, expr);
    }

    return err;
}

//
// block
//     : OPEN_BRACE expressions CLOSE_BRACE
//     ;
static errno_t Parser_Block(Parser* _Nonnull self, Block* _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();
    Block* block = NULL;

    try(Block_Create(self->allocator, &block));

    try(match(kToken_OpeningBrace));
    try(Parser_ExpressionList(self, &block->exprs, '}', true));
    try(match(kToken_ClosingBrace));

    *pOutBlock = block;
    return EOK;

catch:
    *pOutBlock = NULL;
    return err;
}

//
// script
//     : expressions EOF
//     ;
static errno_t Parser_Script(Parser* _Nonnull self, Script* _Nonnull script)
{
    decl_try_err();

    err = Parser_ExpressionList(self, &script->exprs, kToken_Eof, false);
    if (err == EOK) {
        err = match(kToken_Eof);
    }
    return err;
}

// Parses the text 'text' and updates the script object 'script' to reflect the
// result of parsing 'text'.
errno_t Parser_Parse(Parser* _Nonnull self, const char* _Nonnull text, Script* _Nonnull script)
{
    decl_try_err();

    Lexer_SetInput(&self->lexer, text);
    self->allocator = script->allocator;
    self->constantsPool = script->constantsPool;

    err = Parser_Script(self, script);
    
    self->constantsPool = NULL;
    self->allocator = NULL;
    Lexer_SetInput(&self->lexer, NULL);

    return err;
}
