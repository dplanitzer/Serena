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

//
// Notes:
//
// - Errors:
//   An interpreter function returns an error to its caller when it detects some
//   problem that stops it from being able to continue. It may leave the op-stack
//   in an undetermined state in this case. Errors are propagated up the call
//   chain to the next closest try-catch construct. The try-catch will clean up
//   the op-stack by dropping everything that was pushed onto the stack since the
//   try-catch invocation and it then continues with the catch block. Errors
//   propagate all the way up to the interpreter entry point if there's no try-
//   catch that wants to catch the error. The interpreter entry point drops
//   everything from the op-stack. Note that once we detect an error, that we do
//   not want to continue executing any more code because we do not want to
//   trigger unexpected side-effects, i.e. by executing an external command that
//   we should not execute because the invocation is lexically after the point
//   at which we've detected the error.
//
// - Expressions:
//   Every expression is expected to leave a single result value on the op-stack.
//   This value will be consumed by the parent expression. The value of a top-level
//   expression is printed to the console if the value is not Void.
//

static errno_t Interpreter_DeclareInternalCommands(InterpreterRef _Nonnull self);
static errno_t Interpreter_DeclareEnvironmentVariables(InterpreterRef _Nonnull self);
static errno_t Interpreter_CompoundString(InterpreterRef _Nonnull self, CompoundString* _Nonnull str);
static errno_t Interpreter_ArithmeticExpression(InterpreterRef _Nonnull self, Arithmetic* _Nonnull expr);
static inline errno_t Interpreter_Block(InterpreterRef _Nonnull self, Block* _Nonnull block);


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
    Value val;
    ProcessArguments* pargs = Process_GetArguments();
    char** envp = pargs->envp;

    while (*envp) {
        char* keyp = *envp;
        char* eqp = strchr(keyp, '=');
        char* valp = (eqp) ? eqp + 1 : NULL;

        if (valp) {
            Value_InitCString(&val, valp, kValueFlag_NoCopy);

            *eqp = '\0';
            err = RunStack_DeclareVariable(self->runStack, kVarModifier_Public | kVarModifier_Mutable, "global", keyp, &val);
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

static errno_t Interpreter_PushVariable(InterpreterRef _Nonnull self, VarRef* _Nonnull vref)
{
    Variable* varp = RunStack_GetVariable(self->runStack, vref->scope, vref->name);
    
    return (varp) ? OpStack_Push(self->opStack, &varp->value) : EUNDEFVAR;
}

static bool Interpreter_ExecuteInternalCommand(InterpreterRef _Nonnull self, int argc, char** argv, char** envp)
{
    Name* np = NameTable_GetName(self->nameTable, argv[0]);

    if (np) {
        np->cb(self, argc, argv, envp);
        OpStack_PushVoid(self->opStack);    // XXX for now
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

    // XXX we always return Void for now (will change once we got value capture support)
    OpStack_PushVoid(self->opStack);

catch:
    return (err == ENOENT) ? ENOCMD : err;
}

static errno_t Interpreter_SerializeValue(InterpreterRef _Nonnull self, const Value* vp)
{
    switch (vp->type) {
        case kValue_String:
            return ArgumentVector_AppendBytes(self->argumentVector, Value_GetCharacters(vp), Value_GetLength(vp));

        default:
            return ENOTIMPL;
    }
}

static errno_t Interpreter_SerializeVariable(InterpreterRef _Nonnull self, const VarRef* vref)
{
    Variable* varp = RunStack_GetVariable(self->runStack, vref->scope, vref->name);

    return (varp) ? Interpreter_SerializeValue(self, &varp->value) : EUNDEFVAR;
}

static errno_t Interpreter_SerializeCompoundString(InterpreterRef _Nonnull self, CompoundString* _Nonnull str)
{
    decl_try_err();

    err = Interpreter_CompoundString(self, str);
    if (err == EOK) {
        const Value* vp = OpStack_GetTos(self->opStack);
        err = ArgumentVector_AppendBytes(self->argumentVector, Value_GetCharacters(vp), Value_GetLength(vp));
    }
    OpStack_Pop(self->opStack);
    
    return err;
}

static errno_t Interpreter_SerializeInteger(InterpreterRef _Nonnull self, int32_t i32)
{
    char buf[__INT_MAX_BASE_10_DIGITS + 1];

    itoa(i32, buf, 10);
    return ArgumentVector_AppendBytes(self->argumentVector, buf, strlen(buf));
}

static errno_t Interpreter_SerializeArithmeticExpression(InterpreterRef _Nonnull self, Arithmetic* _Nonnull expr)
{
    decl_try_err();

    err = Interpreter_ArithmeticExpression(self, expr);
    if (err == EOK) {
        Value* vp = OpStack_GetTos(self->opStack);

        err = Value_ToString(vp);
        if (err == EOK) {
            err = ArgumentVector_AppendBytes(self->argumentVector, Value_GetCharacters(vp), Value_GetLength(vp));
            OpStack_Pop(self->opStack);
        }
    }
    return err;
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

        case kAtom_ArithmeticExpression:
            return Interpreter_SerializeArithmeticExpression(self, atom->u.expr);

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

static errno_t Interpreter_Command(InterpreterRef _Nonnull self, CommandArithmetic* _Nonnull cmd)
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

static errno_t Interpreter_CompoundString(InterpreterRef _Nonnull self, CompoundString* _Nonnull str)
{
    decl_try_err();
    Segment* seg = str->segs;
    size_t nComponents = 0;

    while (seg && err == EOK) {
        switch (seg->type) {
            case kSegment_EscapeSequence:
            case kSegment_String:
                err = OpStack_Push(self->opStack, &AS(seg, LiteralSegment)->value);
                break;

            case kSegment_ArithmeticExpression:
                err = Interpreter_ArithmeticExpression(self, AS(seg, ArithmeticSegment)->expr);
                break;

            case kSegment_VarRef:
                err = Interpreter_PushVariable(self, AS(seg, VarRefSegment)->vref);
                break;

            default:
                err = ENOTIMPL;
                break;
        }

        seg = seg->next;
        nComponents++;
    }

    if (err == EOK) {
        if (nComponents > 0) {
            ValueArray_ToString(OpStack_GetNth(self->opStack, nComponents - 1), nComponents);
            OpStack_PopSome(self->opStack, nComponents - 1);
        } else {
            err = OpStack_PushEmptyString(self->opStack);
        }
    }
    return err;
}

static errno_t Interpreter_BoolExpression(InterpreterRef _Nonnull self, Arithmetic* _Nonnull expr, Value* _Nullable * _Nonnull pOutValue)
{
    errno_t err = Interpreter_ArithmeticExpression(self, expr);
    
    if (err == EOK) {
        Value* vp = OpStack_GetTos(self->opStack);

        *pOutValue = vp;
        if (vp->type != kValue_Bool) {
            err = ETYPEMISMATCH;
        }
    }
    return err; 
}

static errno_t Interpreter_Disjunction(InterpreterRef _Nonnull self, Arithmetic* _Nonnull expr)
{
    decl_try_err();
    Value* lhs_r;
    Value* rhs;

    err = Interpreter_BoolExpression(self, AS(expr, BinaryArithmetic)->lhs, &lhs_r);
    if(err == EOK) {
        if (!lhs_r->u.b) {
            err = Interpreter_BoolExpression(self, AS(expr, BinaryArithmetic)->rhs, &rhs);
            if (err == EOK) {
                lhs_r->u.b = rhs->u.b;
                OpStack_Pop(self->opStack);
            }
        }
    }

    return err;
}

static errno_t Interpreter_Conjunction(InterpreterRef _Nonnull self, Arithmetic* _Nonnull expr)
{
    decl_try_err();
    Value* lhs_r;
    Value* rhs;

    err = Interpreter_BoolExpression(self, AS(expr, BinaryArithmetic)->lhs, &lhs_r);
    if (err == EOK) {
        if (lhs_r->u.b) {
            err = Interpreter_BoolExpression(self, AS(expr, BinaryArithmetic)->rhs, &rhs);
            if (err == EOK) {
                lhs_r->u.b = rhs->u.b;
                OpStack_Pop(self->opStack);
            }
        }
    }

    return err;
}

static errno_t Interpreter_BinaryOp(InterpreterRef _Nonnull self, Arithmetic* _Nonnull expr)
{
    decl_try_err();

    err = Interpreter_ArithmeticExpression(self, AS(expr, BinaryArithmetic)->lhs);
    if (err == EOK) {
        err = Interpreter_ArithmeticExpression(self, AS(expr, BinaryArithmetic)->rhs);
        if (err == EOK) {
            err = Value_BinaryOp(OpStack_GetNth(self->opStack, 1), OpStack_GetTos(self->opStack), expr->type - kArithmetic_Equals);
            OpStack_Pop(self->opStack);
        }
    }
    return err;
}

static errno_t Interpreter_UnaryOp(InterpreterRef _Nonnull self, Arithmetic* _Nonnull expr)
{
    decl_try_err();

    err = Interpreter_ArithmeticExpression(self, AS(expr, UnaryArithmetic)->expr);
    if (err == EOK) {
        err = Value_UnaryOp(OpStack_GetTos(self->opStack), expr->type - kArithmetic_Negative);
    }
    return err;
}

static errno_t Interpreter_IfThen(InterpreterRef _Nonnull self, IfArithmetic* _Nonnull expr)
{
    decl_try_err();
    Value* cond_r;

    err = Interpreter_BoolExpression(self, AS(expr, BinaryArithmetic)->lhs, &cond_r);
    if (err == EOK) {
        const bool isTrue = cond_r->u.b;

        OpStack_Pop(self->opStack);

        if (isTrue) {
            // Execute the then block
            err = Interpreter_Block(self, expr->thenBlock);
        }
        else {
            // Execute the else block if it exists and push Undefined otherwise
            if (expr->elseBlock) {
                err = Interpreter_Block(self, expr->elseBlock);
            }
            else {
                OpStack_PushUndefined(self->opStack);
            }
        }
    }
    return err;
}

static errno_t Interpreter_While(InterpreterRef _Nonnull self, WhileArithmetic* _Nonnull expr)
{
    decl_try_err();
    Value* cond_r;
    bool didLoopProduceValue = false;

    self->loopNestingCount++;
    while (true) {
        err = Interpreter_BoolExpression(self, AS(expr, BinaryArithmetic)->lhs, &cond_r);
        if (err != EOK) {
            break;
        }

        const bool isTrue = cond_r->u.b;
        
        OpStack_Pop(self->opStack);
        if (!isTrue) {
            break;
        }

        err = Interpreter_Block(self, expr->body);
        if (err != EOK) {
            if (err == ECONTINUE) {
                continue;
            }
            else if (err == EBREAK) {
                // The break expression already pushed a value on the op-stack
                didLoopProduceValue = true;
                err = EOK;
                break;
            }
            else {
                break;
            }
        }
        didLoopProduceValue = true;
    }
    self->loopNestingCount--;

    if (!didLoopProduceValue) {
        err = OpStack_PushUndefined(self->opStack);
    }

    return err;
}

static errno_t Interpreter_ArithmeticExpression(InterpreterRef _Nonnull self, Arithmetic* _Nonnull expr)
{
    switch (expr->type) {
        case kArithmetic_Pipeline:
            return ENOTIMPL;

        case kArithmetic_Disjunction:
            return Interpreter_Disjunction(self, expr);

        case kArithmetic_Conjunction: 
            return Interpreter_Conjunction(self, expr);

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
            return Interpreter_BinaryOp(self, expr);

        case kArithmetic_Parenthesized:
        case kArithmetic_Positive:
            return Interpreter_ArithmeticExpression(self, AS(expr, UnaryArithmetic)->expr);

        case kArithmetic_Negative:
        case kArithmetic_Not:
            return Interpreter_UnaryOp(self, expr);

        case kArithmetic_Literal:
            return OpStack_Push(self->opStack, &AS(expr, LiteralArithmetic)->value);

        case kArithmetic_CompoundString:
            return Interpreter_CompoundString(self, AS(expr, CompoundStringArithmetic)->string);

        case kArithmetic_VarRef:
            return Interpreter_PushVariable(self, AS(expr, VarRefArithmetic)->vref);
            
        case kArithmetic_Command:
            return Interpreter_Command(self, AS(expr, CommandArithmetic));

        case kArithmetic_If:
            return Interpreter_IfThen(self, AS(expr, IfArithmetic));

        case kArithmetic_While:
            return Interpreter_While(self, AS(expr, WhileArithmetic));

        default:
            return ENOTIMPL;
    }
}

// Supported assignment forms:
//  $VAR_NAME = expr
static errno_t Interpreter_Assignment(InterpreterRef _Nonnull self, AssignmentExpression* _Nonnull stmt)
{
    if (stmt->lvalue->type != kArithmetic_VarRef) {
        return ENOTLVALUE;
    }

    VarRef* lvref = AS(stmt->lvalue, VarRefArithmetic)->vref;
    Variable* lvar = RunStack_GetVariable(self->runStack, lvref->scope, lvref->name);
    if (lvar == NULL) {
        return EUNDEFVAR;
    }
    if ((lvar->modifiers & kVarModifier_Mutable) == 0) {
        return EIMMUTABLE;
    }

    errno_t err = Interpreter_ArithmeticExpression(self, stmt->rvalue);
    if (err == EOK) {
        lvar->value = *OpStack_GetTos(self->opStack);
        OpStack_Pop(self->opStack);

        // Result is Void
        err = OpStack_PushVoid(self->opStack);
    }

    return err;
}

static errno_t Interpreter_VarDeclExpression(InterpreterRef _Nonnull self, VarDeclExpression* _Nonnull decl)
{
    decl_try_err();
    
    err = Interpreter_ArithmeticExpression(self, decl->expr);
    if (err == EOK) {
        const Value* vp = OpStack_GetTos(self->opStack);

        if (vp->type == kValue_Void || vp->type == kValue_Undefined) {
            return ENOASSIGN;
        }

        err = RunStack_DeclareVariable(self->runStack, decl->modifiers, decl->vref->scope, decl->vref->name, vp);
        if (err == EOK) {
            OpStack_Pop(self->opStack);

            // Result is Void
            err = OpStack_PushVoid(self->opStack);
        }
    }

    return err;
}

static errno_t Interpreter_BreakExpression(InterpreterRef _Nonnull self, BreakExpression* _Nonnull expr)
{
    decl_try_err();

    if (self->loopNestingCount == 0) {
        return ENOTLOOP;
    }

    if (expr->expr) {
        err = Interpreter_ArithmeticExpression(self, AS(expr->expr, Arithmetic));
    }
    else {
        err = OpStack_PushVoid(self->opStack);
    }

    return (err == EOK) ? EBREAK : err;
}

static errno_t Interpreter_Expression(InterpreterRef _Nonnull self, Expression* _Nonnull expr)
{
    switch (expr->type) {
        case kExpression_Null:
            return OpStack_PushVoid(self->opStack);

        case kExpression_ArithmeticExpression:
            return Interpreter_ArithmeticExpression(self, AS(expr, ArithmeticExpression)->expr);

        case kExpression_Assignment:
            return Interpreter_Assignment(self, AS(expr, AssignmentExpression));

        case kExpression_VarDecl:
            return Interpreter_VarDeclExpression(self, AS(expr, VarDeclExpression));

        case kExpression_Continue:
            // This op does not push a value on the op-stack. It will cause the
            // enclosing loop to start the next iteration
            return (self->loopNestingCount > 0) ? ECONTINUE : ENOTLOOP;

        case kExpression_Break:
            return Interpreter_BreakExpression(self, AS(expr, BreakExpression));

        default:
            return ENOTIMPL;
    }
}

static errno_t Interpreter_ExpressionList(InterpreterRef _Nonnull self, ExpressionList* _Nonnull exprList)
{
    decl_try_err();
    Expression* expr = exprList->exprs;

    while (expr) {
        err = Interpreter_Expression(self, expr);
        if (err != EOK) {
            break;
        }

        expr = expr->next;
        if (expr) {
            // All results except the result of the last expression are implicitly
            // removed from the op-stack
            OpStack_Pop(self->opStack);
        }
    }

    return err;
}

static inline errno_t Interpreter_Block(InterpreterRef _Nonnull self, Block* _Nonnull block)
{
    return Interpreter_ExpressionList(self, &block->exprs);
}

// Prints and pops the value on top of the op-stack
static void Interpreter_PrintResult(InterpreterRef _Nonnull self)
{
    const Value* rp = OpStack_GetTos(self->opStack);

    switch (rp->type) {
        case kValue_Void:
            break;

        case kValue_Undefined:
            puts("Undefined");
            break;

        default:
            Value_Write(rp, stdout);
            putchar('\n');
            break;
    }

    OpStack_Pop(self->opStack);
}

// Interprets 'script' and executes all its expressions.
errno_t Interpreter_Execute(InterpreterRef _Nonnull self, Script* _Nonnull script, ExecuteOptions options)
{
    decl_try_err();
    const bool bPushScope = (options & kExecute_PushScope) == kExecute_PushScope;
    const bool bPrintResult = (options & kExecute_PrintResult) == kExecute_PrintResult;

    //Script_Print(script);
    //putchar('\n');

    if (bPushScope) {
        err = RunStack_PushScope(self->runStack);
        if (err != EOK) {
            return err;
        }
    }

    err = Interpreter_ExpressionList(self, &script->exprs);
    if (err == EOK && bPrintResult) {
        Interpreter_PrintResult(self);
    }

    if (bPushScope) {
        RunStack_PopScope(self->runStack);
    }

    OpStack_PopAll(self->opStack);
    StackAllocator_DeallocAll(self->allocator);
    return err;
}
