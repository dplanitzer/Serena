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


static errno_t Parser_Expression(Parser* _Nonnull self, Expression* _Nullable * _Nonnull _Nonnull pOutExpr);


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
static errno_t Parser_Command(Parser* _Nonnull self, Command* _Nullable * _Nonnull pOutCmd)
{
    decl_try_err();
    Command* cmd = NULL;
    Expression* expr = NULL;
    Atom* atom = NULL;
    bool done = false;

    try(Command_Create(self->allocator, &cmd));

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
                try(Parser_ParenthesizedExpression(self, &expr));
                try(Atom_CreateWithExpression(self->allocator, expr, &atom));
                break;

            default:
                done = true;
                break;
        }

        if (atom) {
            Command_AddAtom(cmd, atom);
            atom = NULL;
        }
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
// statementTerminator
//     : NL | SEMICOLON | AMPERSAND
//     ;
static errno_t Parser_StatementTerminator(Parser* _Nonnull self, Statement* _Nonnull stmt, bool isScriptLevel)
{
    decl_try_err();
    const Token* t = Lexer_GetToken(&self->lexer);
    
    switch (t->id) {
        case kToken_Newline:
        case kToken_Semicolon:
            stmt->isAsync = false;
            consume();
            break;

        case kToken_Ampersand:
            stmt->isAsync = true;
            consume();
            break;

        default:
            if (isScriptLevel && t->id == kToken_Eof) {
                // Accept scripts where the last line is terminated by EOF
                // since this is what we get in interactive mode anyway
                stmt->isAsync = false;
                break; 
            }
            err = ESYNTAX;
            break;
    }

    return err;
}

//
// statement
//     : (varDeclaration
//         | assignmentStatement
//         | expression) statementTerminator
//     ;
static errno_t Parser_Statement(Parser* _Nonnull self, StatementList* _Nonnull stmts, bool isScriptLevel)
{
    decl_try_err();
    Statement* stmt = NULL;

    try(Statement_Create(self->allocator, &stmt));

    Expression* expr = NULL;
    try(Parser_Expression(self, &expr));
    Statement_SetExpression(stmt, expr);

    try(Parser_StatementTerminator(self, stmt, isScriptLevel));
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
