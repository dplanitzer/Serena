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
static errno_t Parser_EscapedExpression(Parser* _Nonnull self, Expression* _Nullable * _Nonnull pOutExpr);
static errno_t Parser_ParenthesizedExpression(Parser* _Nonnull self, Expression* _Nullable * _Nonnull pOutExpr);
static errno_t Parser_Expression(Parser* _Nonnull self, Expression* _Nullable * _Nonnull _Nonnull pOutExpr);


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
//         | escapedExpression(dq_mode)
//         | VAR_NAME(dq_mode)
//       )* DOUBLE_QUOTE(dq_mode)
//     ;
static errno_t Parser_QuotedString(Parser* _Nonnull self, bool isBacktick, QuotedString* _Nullable * _Nonnull pOutStr)
{
    decl_try_err();
    QuotedString* str = NULL;
    StringAtom* atom = NULL;
    TokenId quoteToken = (isBacktick) ? kToken_DoubleBacktick : kToken_DoubleQuote;
    LexerMode lexerMode = (isBacktick) ? kLexerMode_DoubleBacktick : kLexerMode_DoubleQuote;
    bool done = false;

    try(matchAndSwitchMode(quoteToken, lexerMode));
    try(QuotedString_Create(self->allocator, &str));

    while(!done) {
        const Token* t = Lexer_GetToken(&self->lexer);

        switch (t->id) {
            case kToken_StringSegment:
                try(StringAtom_CreateWithString(self->allocator, kStringAtom_Segment, t->u.string, t->length, &atom));
                consume();
                break;

            case kToken_EscapeSequence:
                try(failOnIncomplete(t));
                try(StringAtom_CreateWithString(self->allocator, kStringAtom_EscapeSequence, t->u.string, t->length, &atom));
                consume();
                break;

            case kToken_EscapedExpression: {
                Expression* expr = NULL;
                try(Parser_EscapedExpression(self, &expr));
                try(StringAtom_CreateWithExpression(self->allocator, expr, &atom));
                break;
            }

            case kToken_VariableName: {
                VarRef* vref = NULL;
                try(Parser_VarReference(self, &vref));
                try(StringAtom_CreateWithVarRef(self->allocator, vref, &atom));
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

        if (atom) {
            QuotedString_AddAtom(str, atom);
            atom = NULL;
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
// escapedExpression
//     : ESCAPED_EXPRESSION expression CLOSE_PARA
//     ;
static errno_t Parser_EscapedExpression(Parser* _Nonnull self, Expression* _Nullable * _Nonnull pOutExpr)
{
    decl_try_err();
    LexerMode savedMode = Lexer_GetMode(&self->lexer);
    Expression* expr = NULL;

    try(matchAndSwitchMode(kToken_EscapedExpression, kLexerMode_Default));
    try(Parser_Expression(self, &expr));
    try(matchAndSwitchMode(kToken_ClosingParenthesis, savedMode));

    *pOutExpr = expr;
    return EOK;

catch:
    Lexer_SetMode(&self->lexer, savedMode);
    *pOutExpr = NULL;
    return err;
}

//
// parenthesizedExpression
//     : OPEN_PARA expression CLOSE_PARA
//     ;
static errno_t Parser_ParenthesizedExpression(Parser* _Nonnull self, Expression* _Nullable * _Nonnull pOutExpr)
{
    decl_try_err();
    Expression* expr = NULL;

    try(match(kToken_OpeningParenthesis));
    try(Parser_Expression(self, &expr));
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
            QuotedString* str = NULL;
            try(Parser_QuotedString(self, true, &str));
            try(Atom_CreateWithQuotedString(self->allocator, kAtom_DoubleBacktickString, str, hasLeadingWhitespace, pOutAtom));
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
//     | IF
//     | INTERNAL
//     | LET
//     | VAR
//     | WHILE
//     | PUBLIC
//     | ASSIGNMENT
//     | CONJUNCTION
//     | DISJUNCTION
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
//     | parenthesizedExpression
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
            QuotedString* str = NULL;
            try(Parser_QuotedString(self, true, &str));
            try(Atom_CreateWithQuotedString(self->allocator, kAtom_DoubleBacktickString, str, hasLeadingWhitespace, pOutAtom));
            break;
        }

        case kToken_SingleQuoteString:
            try(failOnIncomplete(t));
            try(Atom_CreateWithString(self->allocator, kAtom_SingleQuoteString, t->u.string, t->length, hasLeadingWhitespace, pOutAtom));
            consume();
            break;

        case kToken_DoubleQuote: {
            QuotedString* str = NULL;
            try(Parser_QuotedString(self, t->id == kToken_DoubleBacktick, &str));
            try(Atom_CreateWithQuotedString(self->allocator, kAtom_DoubleQuoteString, str, hasLeadingWhitespace, pOutAtom));
            break;
        }

        case kToken_Integer:
            try(Atom_CreateWithInteger(self->allocator, t->u.i32, hasLeadingWhitespace, pOutAtom));
            consume();
            break;

        case kToken_Identifier:
        case kToken_Else:
        case kToken_If:
        case kToken_Internal:
        case kToken_Let:
        case kToken_Public:
        case kToken_Var:
        case kToken_While:
            try(failOnIncomplete(t));
            try(Atom_CreateWithString(self->allocator, kAtom_Identifier, t->u.string, t->length, hasLeadingWhitespace, pOutAtom));
            consume();
            break;

        case kToken_OpeningParenthesis: {
            Expression* expr = NULL;
            try(Parser_ParenthesizedExpression(self, &expr));
            try(Atom_CreateWithExpression(self->allocator, expr, hasLeadingWhitespace, pOutAtom));
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
static errno_t Parser_Command(Parser* _Nonnull self, Command* _Nullable * _Nonnull pOutCmd)
{
    decl_try_err();
    Command* cmd = NULL;
    Atom* atom = NULL;

    try(Command_Create(self->allocator, &cmd));


    // The primary fragment is required
    try(Parser_CommandPrimaryFragment(self, &atom));
    if (atom == NULL) {
        throw(ESYNTAX);
    }

    Command_AddAtom(cmd, atom);


    // The secondary fragment is optional
    for (;;) {
        try(Parser_CommandSecondaryFragment(self, &atom));
        if (atom == NULL) {
            break;
        }

        Command_AddAtom(cmd, atom);
        atom = NULL;
    }

    *pOutCmd = cmd;
    return EOK;

catch:
    *pOutCmd = NULL;
    return err;
}

//
// expression
//     : (disjunction | command) (BAR command)*
//     ;
static errno_t Parser_Expression(Parser* _Nonnull self, Expression* _Nullable * _Nonnull pOutExpr)
{
    decl_try_err();
    Expression* expr = NULL;
    Command* cmd = NULL;

    try(Expression_Create(self->allocator, &expr));

    try(Parser_Command(self, &cmd));
    Expression_AddCommand(expr, cmd);
    cmd = NULL;

    while (peek('|')) {
        consume();

        try(Parser_Command(self, &cmd));
        Expression_AddCommand(expr, cmd);
        cmd = NULL;
    }

    *pOutExpr = expr;
    return EOK;

catch:
    *pOutExpr = NULL;
    return err;
}

//
// varDeclaration
//     : (INTERNAL | PUBLIC)? (LET | VAR) VAR_NAME ASSIGNMENT expression
//     ;
static errno_t Parser_VarDeclaration(Parser* _Nonnull self, Statement* _Nullable * _Nonnull pOutDecl)
{
    decl_try_err();
    VarRef* vref = NULL;
    Expression* expr = NULL;
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
    try(Parser_Expression(self, &expr));

    try(Statement_CreateVarDecl(self->allocator, modifiers, vref, expr, pOutDecl));
    return EOK;

catch:
    *pOutDecl = NULL;
    return err;
}

//
// statementTerminator
//     : NL | SEMICOLON | AMPERSAND
//     ;
static errno_t Parser_StatementTerminator(Parser* _Nonnull self, bool isScriptLevel, bool* _Nonnull pOutIsAsync)
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

            // Optimization: we don't want to generate statements for '<null>\n'
            while (peek(kToken_Newline)) {
                consume();
            }
            break;

        default:
            if (isScriptLevel && t->id == kToken_Eof) {
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
// statement
//     : (varDeclaration
//         | expression (ASSIGNMENT expression)? )? statementTerminator
//     ;
static errno_t Parser_Statement(Parser* _Nonnull self, StatementList* _Nonnull stmts, bool isScriptLevel)
{
    decl_try_err();
    Statement* stmt = NULL;

    switch (Lexer_GetToken(&self->lexer)->id) {
        case kToken_Newline:
        case kToken_Semicolon:
        case kToken_Ampersand:
        case kToken_Eof:
            // Null statement
            try(Statement_CreateNull(self->allocator, &stmt));
            break;

        case kToken_Public:
        case kToken_Internal:
        case kToken_Let:
        case kToken_Var:
            // Variable declaration
            try(Parser_VarDeclaration(self, &stmt));
            break;

        default: {
            // Mathematical expression or command
            Expression* expr = NULL;
            try(Parser_Expression(self, &expr));

            // Check for assignment statement
            if (peek(kToken_Assignment)) {
                Expression* rvalue = NULL;

                consume();
                try(Parser_Expression(self, &rvalue));
                try(Statement_CreateAssignment(self->allocator, expr, rvalue, &stmt));
            }
            else {
                try(Statement_CreateExpression(self->allocator, expr, &stmt));
            }
        }
    }

    // Required statement terminator
    try(Parser_StatementTerminator(self, isScriptLevel, &stmt->isAsync));
    StatementList_AddStatement(stmts, stmt);

catch:
    return err;
}

//
// statementList:
//     : statement*
//     ;
static errno_t Parser_StatementList(Parser* _Nonnull self, StatementList* _Nonnull stmts, int endToken, bool isScriptLevel)
{
    decl_try_err();

    while (err == EOK && !peek(endToken)) {
        err = Parser_Statement(self, stmts, isScriptLevel);
    }

    return err;
}

//
// block
//     : OPEN_BRACE statementList CLOSE_BRACE
//     ;
static errno_t Parser_Block(Parser* _Nonnull self, Block* _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();
    Block* block = NULL;

    try(Block_Create(self->allocator, &block));

    try(match(kToken_OpeningBrace));
    try(Parser_StatementList(self, &block->statements, '}', false));
    try(match(kToken_ClosingBrace));

    *pOutBlock = block;
    return EOK;

catch:
    *pOutBlock = NULL;
    return err;
}

//
// script
//     : statementList EOF
//     ;
static errno_t Parser_Script(Parser* _Nonnull self, Script* _Nonnull script)
{
    decl_try_err();

    err = Parser_StatementList(self, &script->statements, kToken_Eof, true);
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
    err = Parser_Script(self, script);
    self->allocator = NULL;
    Lexer_SetInput(&self->lexer, NULL);

    return err;
}
