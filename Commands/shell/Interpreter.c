//
//  Interpreter.c
//  sh
//
//  Created by Dietmar Planitzer on 1/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Errors.h"
#include "builtins/Builtins.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <System/System.h>

static errno_t Interpreter_DeclareInternalCommands(InterpreterRef _Nonnull self);
static errno_t Interpreter_DeclareEnvironmentVariables(InterpreterRef _Nonnull self);
static errno_t Interpreter_Expression(InterpreterRef _Nonnull self, Expression* _Nonnull expr);


////////////////////////////////////////////////////////////////////////////////

errno_t Interpreter_Create(LineReaderRef _Nonnull lineReader, InterpreterRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    InterpreterRef self;
    
    try_null(self, calloc(1, sizeof(Interpreter)), ENOMEM);
    try(StackAllocator_Create(1024, 8192, &self->allocator));
    self->lineReader = lineReader;

    try(NameTable_Create(&self->nameTable));
    try(Interpreter_DeclareInternalCommands(self));
    
    try(OpStack_Create(&self->opStack));

    try(RunStack_Create(&self->runStack));
    try(Interpreter_DeclareEnvironmentVariables(self));
    
    try(EnvironCache_Create(self->runStack, &self->environCache));
    try(ArgumentVector_Create(&self->argumentVector));

    *pOutSelf = self;
    return 0;

catch:
    Interpreter_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void Interpreter_Destroy(InterpreterRef _Nullable self)
{
    if (self) {
        ArgumentVector_Destroy(self->argumentVector);
        self->argumentVector = NULL;

        EnvironCache_Destroy(self->environCache);
        self->environCache = NULL;

        RunStack_Destroy(self->runStack);
        self->runStack = NULL;

        OpStack_Destroy(self->opStack);
        self->opStack = NULL;
        
        NameTable_Destroy(self->nameTable);
        self->nameTable = NULL;

        StackAllocator_Destroy(self->allocator);
        self->allocator = NULL;

        self->lineReader = NULL;
        free(self);
    }
}

// Returns the number of entries that currently exist in the history.
int Interpreter_GetHistoryCount(InterpreterRef _Nonnull self)
{
    return (self->lineReader) ? LineReader_GetHistoryCount(self->lineReader) : 0;
}

// Returns a reference to the history entry at the given index. Entries are
// ordered ascending from oldest to newest.
const char* _Nonnull Interpreter_GetHistoryAt(InterpreterRef _Nonnull self, int idx)
{
    return (self->lineReader) ? LineReader_GetHistoryAt(self->lineReader, idx) : "";
}


static errno_t Interpreter_DeclareInternalCommands(InterpreterRef _Nonnull self)
{
    decl_try_err();

    try(NameTable_DeclareName(self->nameTable, "cd", cmd_cd));
    try(NameTable_DeclareName(self->nameTable, "cls", cmd_cls));
    try(NameTable_DeclareName(self->nameTable, "delete", cmd_delete));
    try(NameTable_DeclareName(self->nameTable, "echo", cmd_echo));
    try(NameTable_DeclareName(self->nameTable, "exit", cmd_exit));
    try(NameTable_DeclareName(self->nameTable, "history", cmd_history));
    try(NameTable_DeclareName(self->nameTable, "list", cmd_list));
    try(NameTable_DeclareName(self->nameTable, "makedir", cmd_makedir));
    try(NameTable_DeclareName(self->nameTable, "pwd", cmd_pwd));
    try(NameTable_DeclareName(self->nameTable, "rename", cmd_rename));
    try(NameTable_DeclareName(self->nameTable, "type", cmd_type));

catch:
    return err;
}

static errno_t Interpreter_DeclareEnvironmentVariables(InterpreterRef _Nonnull self)
{
    decl_try_err();
    RawData vdat;
    ProcessArguments* pargs = Process_GetArguments();
    char** envp = pargs->envp;

    while (*envp) {
        char* keyp = *envp;
        char* eqp = strchr(keyp, '=');
        char* valp = (eqp) ? eqp + 1 : NULL;

        if (valp) {
            vdat.string.characters = valp;
            vdat.string.length = strlen(valp);

            *eqp = '\0';
            err = RunStack_DeclareVariable(self->runStack, kVarModifier_Public | kVarModifier_Mutable, "global", keyp, kValue_String, vdat);
            *eqp = '=';
            // We ignore non-fatal errors here and simply drop the erroneous
            // environment variable because we don't want the shell to die over
            // e.g. a simple redefinition...
            if (err == ENOMEM) {
                return err;
            }
        }

        envp++;
    }

    return EOK;
}

static bool Interpreter_ExecuteInternalCommand(InterpreterRef _Nonnull self, int argc, char** argv, char** envp)
{
    Name* np = NameTable_GetName(self->nameTable, argv[0]);

    if (np) {
        np->cb(self, argc, argv, envp);
        return true;
    }

    return false;
}

static errno_t Interpreter_ExecuteExternalCommand(InterpreterRef _Nonnull self, int argc, char** argv, char** envp)
{
    static const char* gSearchPath = "/System/Commands/";

    decl_try_err();
    ProcessId childPid;
    char* cmdPath = NULL;
    const bool isAbsPath = argv[0][0] == '/';
    const size_t searchPathLen = (isAbsPath) ? 0 : strlen(gSearchPath);
    SpawnOptions opts = {0};
    
    const size_t cmdPathLen = searchPathLen + strlen(argv[0]);
    if (cmdPathLen > PATH_MAX - 1) {
        throw(ENAMETOOLONG);
    }

    try_null(cmdPath, StackAllocator_Alloc(self->allocator, sizeof(char*) * (cmdPathLen + 1)), ENOMEM);
    if (!isAbsPath) {
        strcpy(cmdPath, gSearchPath);
        strcat(cmdPath, argv[0]);
    }
    else {
        strcpy(cmdPath, argv[0]);
    }

    opts.envp = envp;


    // Spawn the external command
    try(Process_Spawn(cmdPath, argv, &opts, &childPid));


    // Wait for the command to complete its task
    ProcessTerminationStatus pts;
    Process_WaitForTerminationOfChild(childPid, &pts);

catch:
    return (err == ENOENT) ? ENOCMD : err;
}

static errno_t Interpreter_SerializeValue(InterpreterRef _Nonnull self, const Value* vp)
{
    switch (vp->type) {
        case kValue_String:
            return ArgumentVector_AppendBytes(self->argumentVector, vp->u.string.characters, vp->u.string.length);

        default:
            return ENOTIMPL;
    }
}

static errno_t Interpreter_SerializeVariable(InterpreterRef _Nonnull self, const VarRef* vref)
{
    Variable* varp = RunStack_GetVariable(self->runStack, vref->scope, vref->name);

    return (varp) ? Interpreter_SerializeValue(self, &varp->value) : EUNDEFINED;
}

static errno_t Interpreter_SerializeCompoundString(InterpreterRef _Nonnull self, CompoundString* _Nonnull str)
{
    decl_try_err();
    StringAtom* atom = str->atoms;

    while (atom && err == EOK) {
        switch (atom->type) {
            case kStringAtom_EscapeSequence:
            case kStringAtom_Segment:
                err = ArgumentVector_AppendBytes(self->argumentVector, StringAtom_GetString(atom), StringAtom_GetStringLength(atom));
                break;

            case kStringAtom_Expression:
                break;

            case kStringAtom_VariableReference:
                err = Interpreter_SerializeVariable(self, atom->u.vref);
                break;

            default:
                err = ENOTIMPL;
                break;
        }
        atom = atom->next;
    }
    return err;
}

static errno_t Interpreter_SerializeInteger(InterpreterRef _Nonnull self, int32_t i32)
{
    char buf[__INT_MAX_BASE_10_DIGITS + 1];

    itoa(i32, buf, 10);
    return ArgumentVector_AppendBytes(self->argumentVector, buf, strlen(buf));
}

static errno_t Interpreter_SerializeCommandFragment(InterpreterRef _Nonnull self, Atom* _Nonnull atom)
{
    switch (atom->type) {
        case kAtom_BacktickString:
        case kAtom_SingleQuoteString:
        case kAtom_Identifier:
            return ArgumentVector_AppendBytes(self->argumentVector, Atom_GetString(atom), Atom_GetStringLength(atom));

        case kAtom_Integer:
            return Interpreter_SerializeInteger(self, atom->u.i32);

        case kAtom_DoubleBacktickString:
        case kAtom_DoubleQuoteString:
            return Interpreter_SerializeCompoundString(self, atom->u.qstring);

        case kAtom_VariableReference:
            return Interpreter_SerializeVariable(self, atom->u.vref);

        case kAtom_Expression:
            return ENOTIMPL;

        default:
            return ENOTIMPL;
    }
}

// XXX Serialization should grab the original text that appears in the input line.
// XXX To make this work however, we first need source ranges in the intermediate
// XXX representation. Once this is there we can fix problems like 'echo 32232323213213'
// XXX which overflows teh i32 representation and thus the echo prints INT32_MAX
// XXX instead of the expected integer. Once we got teh source ranges we can associate
// XXX the original too-big-number with the converted number and the serialization
// XXX can then serialize the original number the way it was written. This will also
// XXX then take care of subtle differences like Unicode chars were not normalized
// XXX in the source but are normalized after lexing, etc.
static errno_t Interpreter_SerializeCommand(InterpreterRef _Nonnull self, Atom* _Nonnull atoms, bool* _Nonnull pOutIsForcedExternal)
{
    decl_try_err();
    Atom* atom = atoms;
    bool isFirstArg = true;

    *pOutIsForcedExternal = false;
    ArgumentVector_Open(self->argumentVector);

    while (atom && err == EOK) {
        bool isFirstAtom = true;

        // We always pick up the first atom in an non-whitespace-separated-atom-sequence
        // The 2nd, 3rd, etc we only pick up if they don't have leading whitespace
        while (atom && (!atom->hasLeadingWhitespace || isFirstAtom)) {
            try(Interpreter_SerializeCommandFragment(self, atom));

            if (isFirstArg && (atom->type == kAtom_BacktickString || atom->type == kAtom_DoubleBacktickString)) {
                *pOutIsForcedExternal = true;
            }

            atom = atom->next;
            isFirstAtom = false;
        }

        try(ArgumentVector_EndOfArg(self->argumentVector));
        isFirstArg = false;
    }

    try(ArgumentVector_Close(self->argumentVector));

catch:
    return err;
}

static errno_t Interpreter_Command(InterpreterRef _Nonnull self, CommandExpression* _Nonnull cmd)
{
    decl_try_err();
    bool isForcedExternal;

    // Create the command argument vector by converting all atoms in the command
    // expression into argument strings.
    try(Interpreter_SerializeCommand(self, cmd->atoms, &isForcedExternal));
    
    const int argc = ArgumentVector_GetArgc(self->argumentVector);
    char** argv = ArgumentVector_GetArgv(self->argumentVector);
    char** envp = EnvironCache_GetEnvironment(self->environCache);


    // Check whether this is a builtin command and execute it, if so
    if (!isForcedExternal) {
        if (Interpreter_ExecuteInternalCommand(self, argc, argv, envp)) {
            return EOK;
        }
    }


    // Not a builtin command. Look for an external command
    err = Interpreter_ExecuteExternalCommand(self, argc, argv, envp);

catch:
    return err;
}

static errno_t eval_bool_expr(InterpreterRef _Nonnull self, Expression* _Nonnull expr, Value* _Nullable * _Nullable pOutValue)
{
    decl_try_err();

    err = Interpreter_Expression(self, expr);
    if (err == EOK) {
        Value* lhs_r = OpStack_GetTos(self->opStack);

        if (lhs_r->type == kValue_Bool) {
            if (pOutValue) *pOutValue = lhs_r;
        } else {
            if (pOutValue) *pOutValue = NULL;
            err = ETYPEMISMATCH;
        }
    }
    return err; 
}

static errno_t Interpreter_Expression(InterpreterRef _Nonnull self, Expression* _Nonnull expr)
{
    decl_try_err();

    switch (expr->type) {
        case kExpression_Pipeline:
            return ENOTIMPL;

        case kExpression_Disjunction: {
            bool lhsIsTrue = false, rhsIsTrue = false;
            Value* lhs_r;

            err = eval_bool_expr(self, AS(expr, BinaryExpression)->lhs, &lhs_r);
            if(err == EOK) {
                lhsIsTrue = lhs_r->u.b;

                if (!lhsIsTrue) {
                    err = eval_bool_expr(self, AS(expr, BinaryExpression)->rhs, NULL);
                    if (err == EOK) {
                        rhsIsTrue = OpStack_GetTos(self->opStack)->u.b;
                        OpStack_Pop(self->opStack, 1);
                    }
                }
            }

            if (err == EOK) {
                lhs_r->u.b = lhsIsTrue || rhsIsTrue;
            } else {
                OpStack_Pop(self->opStack, 1);
            }
            return err;
        }

        case kExpression_Conjunction: {
            bool lhsIsTrue = false, rhsIsTrue = false;
            Value* lhs_r;

            err = eval_bool_expr(self, AS(expr, BinaryExpression)->lhs, &lhs_r);
            if (err == EOK) {
                lhsIsTrue = lhs_r->u.b;

                if (lhsIsTrue) {
                    err = eval_bool_expr(self, AS(expr, BinaryExpression)->rhs, NULL);
                    if (err == EOK) {
                        rhsIsTrue = OpStack_GetTos(self->opStack)->u.b;
                        OpStack_Pop(self->opStack, 1);
                    }
                }
            }

            if (err == EOK) {
                lhs_r->u.b = lhsIsTrue && rhsIsTrue;
            } else {
                OpStack_Pop(self->opStack, 1);
            }
            return err;
        }

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
            Interpreter_Expression(self, AS(expr, BinaryExpression)->lhs);
            Interpreter_Expression(self, AS(expr, BinaryExpression)->rhs);
            err = Value_BinaryOp(OpStack_GetNth(self->opStack, 1), OpStack_GetTos(self->opStack), expr->type - kExpression_Equals);
            OpStack_Pop(self->opStack, 1);
            return err;

        case kExpression_Parenthesized:
        case kExpression_Positive:
            return Interpreter_Expression(self, AS(expr, UnaryExpression)->expr);

        case kExpression_Negative:
        case kExpression_Not:
            Interpreter_Expression(self, AS(expr, UnaryExpression)->expr);
            return Value_UnaryOp(OpStack_GetTos(self->opStack), expr->type - kExpression_Negative);

        case kExpression_Literal:
            return OpStack_Push(self->opStack, &AS(expr, LiteralExpression)->value);

        case kExpression_CompoundString:
        case kExpression_VarRef:
            return ENOTIMPL;
            
        case kExpression_Command:
            return Interpreter_Command(self, AS(expr, CommandExpression));

        case kExpression_If:
        case kExpression_While:
            return ENOTIMPL;

        default:
            return ENOTIMPL;
    }
}

static errno_t Interpreter_Statement(InterpreterRef _Nonnull self, Statement* _Nonnull stmt)
{
    decl_try_err();

    switch (stmt->type) {
        case kStatement_Null:
            return EOK;

        case kStatement_Expression:
            err = Interpreter_Expression(self, AS(stmt, ExpressionStatement)->expr);
            if (err == EOK && !OpStack_IsEmpty(self->opStack)) {
                Value_Write(OpStack_GetTos(self->opStack), stdout);
                putchar('\n');
            }
            OpStack_PopAll(self->opStack);
            break;

        case kStatement_Assignment:
            err = ENOTIMPL;
            break;

        case kStatement_VarDecl: {
            VarDeclStatement* decl = AS(stmt, VarDeclStatement);
            RawData vdat = {.string = {"Not yet", 7}};
            err = RunStack_DeclareVariable(self->runStack, decl->modifiers, decl->vref->scope, decl->vref->name, kValue_String, vdat);  // XXX
            break;
        }

        default:
            err = ENOTIMPL;
            break;
    }
    return err;
}

static errno_t Interpreter_StatementList(InterpreterRef _Nonnull self, StatementList* _Nonnull stmts)
{
    decl_try_err();
    Statement* stmt = stmts->stmts;

    while (stmt && err == EOK) {
        err = Interpreter_Statement(self, stmt);
        stmt = stmt->next;
    }

    return err;
}

static errno_t Interpreter_Block(InterpreterRef _Nonnull self, Block* _Nonnull block)
{
    return Interpreter_StatementList(self, &block->statements);
}

// Interprets 'script' and executes all its statements.
errno_t Interpreter_Execute(InterpreterRef _Nonnull self, Script* _Nonnull script, bool bPushScope)
{
    decl_try_err();

    //Script_Print(script);
    //putchar('\n');

    if (bPushScope) {
        err = RunStack_PushScope(self->runStack);
        if (err != EOK) {
            return err;
        }
    }

    err = Interpreter_StatementList(self, &script->statements);
    
    if (bPushScope) {
        RunStack_PopScope(self->runStack);
    }

    OpStack_PopAll(self->opStack);
    StackAllocator_DeallocAll(self->allocator);
    return err;
}
