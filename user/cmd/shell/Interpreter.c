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
#include <sys/proc.h>
#include <sys/spawn.h>
#include <sys/wait.h>

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

static void Interpreter_DeclareInternalCommands(InterpreterRef _Nonnull self);
static void Interpreter_DeclareEnvironmentVariables(InterpreterRef _Nonnull self);
static errno_t Interpreter_CompoundString(InterpreterRef _Nonnull self, CompoundString* _Nonnull str);
static errno_t Interpreter_ArithmeticExpression(InterpreterRef _Nonnull self, Arithmetic* _Nonnull expr);
static inline errno_t Interpreter_Block(InterpreterRef _Nonnull self, Block* _Nonnull block);


////////////////////////////////////////////////////////////////////////////////

InterpreterRef _Nonnull Interpreter_Create(LineReaderRef _Nonnull lineReader)
{
    InterpreterRef self = calloc(1, sizeof(Interpreter));
    
    self->allocator = StackAllocator_Create(1024, 8192);
    self->lineReader = lineReader;

    self->nameTable = NameTable_Create();
    Interpreter_DeclareInternalCommands(self);
    
    self->opStack = OpStack_Create();

    self->runStack = RunStack_Create();
    Interpreter_DeclareEnvironmentVariables(self);
    
    self->environCache = EnvironCache_Create(self->runStack);
    self->argumentVector = ArgumentVector_Create();

    return self;
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

        CDEntry* cde = self->cdStackTos;
        while (cde) {
            CDEntry* pe = cde->prev;

            free(cde->path);
            free(cde);
            cde = pe;
        }

        self->lineReader = NULL;
        free(self);
    }
}

errno_t Interpreter_IterateVariables(InterpreterRef _Nonnull self, RunStackIterator _Nonnull cb, void* _Nullable context)
{
    return RunStack_Iterate(self->runStack, cb, context);
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


static void Interpreter_DeclareInternalCommands(InterpreterRef _Nonnull self)
{
    NameTable_DeclareName(self->nameTable, "cd", cmd_cd);
    NameTable_DeclareName(self->nameTable, "cls", cmd_cls);
    NameTable_DeclareName(self->nameTable, "echo", cmd_echo);
    NameTable_DeclareName(self->nameTable, "exists", cmd_exists);
    NameTable_DeclareName(self->nameTable, "exit", cmd_exit);
    NameTable_DeclareName(self->nameTable, "history", cmd_history);
    NameTable_DeclareName(self->nameTable, "input", cmd_input);
    NameTable_DeclareName(self->nameTable, "load", cmd_load);
    NameTable_DeclareName(self->nameTable, "popcd", cmd_popcd);
    NameTable_DeclareName(self->nameTable, "pushcd", cmd_pushcd);
    NameTable_DeclareName(self->nameTable, "pwd", cmd_pwd);
    NameTable_DeclareName(self->nameTable, "save", cmd_save);
    NameTable_DeclareName(self->nameTable, "vars", cmd_vars);
}

static void Interpreter_DeclareEnvironmentVariables(InterpreterRef _Nonnull self)
{
    decl_try_err();
    Value val;
    pargs_t* pargs = getpargs();
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
        }

        envp++;
    }
}

static errno_t Interpreter_PushVariable(InterpreterRef _Nonnull self, VarRef* _Nonnull vref)
{
    Variable* varp = RunStack_GetVariable(self->runStack, vref->scope, vref->name);
    
    if (varp) {
        OpStack_Push(self->opStack, &varp->value);
        return EOK;
     }
     else {
        return EUNDEFVAR;
     }
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

static bool should_use_search_path(const char* _Nonnull path)
{
    // Series of dots with at least one slash
    while (*path == '.') {
        path++;
    }

    if (*path == '/') {
        return false;
    }

    return true;
}

static errno_t Interpreter_ExecuteExternalCommand(InterpreterRef _Nonnull self, int argc, char** argv, char** envp)
{
    static const char* gSearchPath = "/System/Commands/";

    decl_try_err();
    pid_t childPid;
    char* cmdPath = NULL;
    const bool needsSearchPath = should_use_search_path(argv[0]);
    const size_t searchPathLen = (needsSearchPath) ? strlen(gSearchPath) : 0;
    spawn_opts_t opts = {0};
    
    const size_t cmdPathLen = searchPathLen + strlen(argv[0]);
    if (cmdPathLen > PATH_MAX - 1) {
        throw(ENAMETOOLONG);
    }

    try_null(cmdPath, StackAllocator_Alloc(self->allocator, sizeof(char*) * (cmdPathLen + 1)), ENOMEM);
    if (needsSearchPath) {
        strcpy(cmdPath, gSearchPath);
        strcat(cmdPath, argv[0]);
    }
    else {
        strcpy(cmdPath, argv[0]);
    }

    opts.envp = envp;


    // Spawn the external command
    if (os_spawn(cmdPath, argv, &opts, &childPid) != 0) {
        throw(errno);
    }


    // Wait for the command to complete its task
    struct proc_status ps;
    proc_join(JOIN_PROC, childPid, &ps);
    if (ps.reason == JREASON_EXCEPTION) {
        fprintf(stderr, "%s crashed: %d\n", argv[0], ps.u.excptno);
    }


    // XXX we always return Void for now (will change once we got value capture support)
    OpStack_PushVoid(self->opStack);

catch:
    return (err == ENOENT) ? ENOCMD : err;
}

static errno_t Interpreter_SerializeValue(InterpreterRef _Nonnull self, const Value* vp)
{
    switch (vp->type) {
        case kValue_String:
            ArgumentVector_AppendBytes(self->argumentVector, Value_GetCharacters(vp), Value_GetLength(vp));
            return EOK;

        case kValue_Bool:
        case kValue_Integer:
        case kValue_Void: {
            OpStack_Push(self->opStack, vp);

            Value* vtos = OpStack_GetTos(self->opStack);

            Value_ToString(vtos);
            ArgumentVector_AppendBytes(self->argumentVector, Value_GetCharacters(vtos), Value_GetLength(vtos));
            OpStack_Pop(self->opStack);
            return EOK;
        }

        case kValue_Never:
            return ENOVAL;

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
        ArgumentVector_AppendBytes(self->argumentVector, Value_GetCharacters(vp), Value_GetLength(vp));
    }
    OpStack_Pop(self->opStack);
    
    return err;
}

static errno_t Interpreter_SerializeInteger(InterpreterRef _Nonnull self, int32_t i32)
{
    char buf[__INT_MAX_BASE_10_DIGITS + 1];

    itoa(i32, buf, 10);
    ArgumentVector_AppendBytes(self->argumentVector, buf, strlen(buf));
    return EOK;
}

static errno_t Interpreter_SerializeArithmeticExpression(InterpreterRef _Nonnull self, Arithmetic* _Nonnull expr)
{
    decl_try_err();

    err = Interpreter_ArithmeticExpression(self, expr);
    if (err == EOK) {
        Value* vp = OpStack_GetTos(self->opStack);

        Value_ToString(vp);
        ArgumentVector_AppendBytes(self->argumentVector, Value_GetCharacters(vp), Value_GetLength(vp));
        OpStack_Pop(self->opStack);
    }
    return err;
}

static errno_t Interpreter_SerializeCommandFragment(InterpreterRef _Nonnull self, Atom* _Nonnull atom)
{
    switch (atom->type) {
        case kAtom_BacktickString:
        case kAtom_SingleQuoteString:
        case kAtom_Identifier:
            ArgumentVector_AppendBytes(self->argumentVector, Atom_GetString(atom), Atom_GetStringLength(atom));
            return EOK;

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
// XXX which overflows the i32 representation and thus the echo prints INT32_MAX
// XXX instead of the expected integer. Once we got the source ranges we can associate
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

        ArgumentVector_EndOfArg(self->argumentVector);
        isFirstArg = false;
    }

    ArgumentVector_Close(self->argumentVector);
    return EOK;

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
                OpStack_Push(self->opStack, &AS(seg, LiteralSegment)->value);
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
            OpStack_PushCString(self->opStack, "");
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
            // Execute the else block if it exists and push Void otherwise
            if (expr->elseBlock) {
                err = Interpreter_Block(self, expr->elseBlock);
            }
            else {
                OpStack_PushVoid(self->opStack);
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

        if (didLoopProduceValue) {
            // Remove the result of the loop body from the previous iteration
            OpStack_Pop(self->opStack);
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
        // The result of a loop that has never executed its loop body is Void
        OpStack_PushVoid(self->opStack);
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
        case kArithmetic_Modulo:
            return Interpreter_BinaryOp(self, expr);

        case kArithmetic_Parenthesized:
        case kArithmetic_Positive:
            return Interpreter_ArithmeticExpression(self, AS(expr, UnaryArithmetic)->expr);

        case kArithmetic_Negative:
        case kArithmetic_Not:
            return Interpreter_UnaryOp(self, expr);

        case kArithmetic_Literal:
            OpStack_Push(self->opStack, &AS(expr, LiteralArithmetic)->value);
            return EOK;

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
        Value_Deinit(&lvar->value);
        Value_InitCopy(&lvar->value, OpStack_GetTos(self->opStack));
        OpStack_Pop(self->opStack);

        // Result is Void
        OpStack_PushVoid(self->opStack);
    }

    return err;
}

static errno_t Interpreter_VarDeclExpression(InterpreterRef _Nonnull self, VarDeclExpression* _Nonnull decl)
{
    decl_try_err();
    
    err = Interpreter_ArithmeticExpression(self, decl->expr);
    if (err == EOK) {
        const Value* vp = OpStack_GetTos(self->opStack);

        err = RunStack_DeclareVariable(self->runStack, decl->modifiers, decl->vref->scope, decl->vref->name, vp);
        if (err == EOK) {
            OpStack_Pop(self->opStack);

            // Result is Void
            OpStack_PushVoid(self->opStack);
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
        OpStack_PushVoid(self->opStack);
    }

    return (err == EOK) ? EBREAK : err;
}

static errno_t Interpreter_Expression(InterpreterRef _Nonnull self, Expression* _Nonnull expr)
{
    switch (expr->type) {
        case kExpression_Null:
            OpStack_PushVoid(self->opStack);
            return EOK;

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

// Prints and pops the value on top of the op-stack
static void Interpreter_PrintResult(InterpreterRef _Nonnull self)
{
    const Value* rp = OpStack_GetTos(self->opStack);

    switch (rp->type) {
        case kValue_Void:
            break;

        case kValue_Never:
            puts("No value");
            break;

        default:
            Value_Write(rp, stdout);
            putchar('\n');
            break;
    }
}

static errno_t Interpreter_ExpressionList(InterpreterRef _Nonnull self, ExpressionList* _Nonnull exprList, bool bPrintResults)
{
    decl_try_err();
    Expression* expr = exprList->exprs;

    if (expr) {
        while (expr) {
            err = Interpreter_Expression(self, expr);
            if (err != EOK) {
                break;
            }

            if (bPrintResults) {
                Interpreter_PrintResult(self);
            }

            expr = expr->next;
            if (expr) {
                // Result of an expression list is the result of the last expression
                OpStack_Pop(self->opStack);
            }
        }
    }
    else {
        // Result of an empty expression list is Void
        OpStack_PushVoid(self->opStack);
    }

    return err;
}

static inline errno_t Interpreter_Block(InterpreterRef _Nonnull self, Block* _Nonnull block)
{
    decl_try_err();

    RunStack_PushScope(self->runStack);
    err = Interpreter_ExpressionList(self, &block->exprs, false);
    RunStack_PopScope(self->runStack);

    return err;
}

// Interprets 'script' and executes all its expressions.
errno_t Interpreter_Execute(InterpreterRef _Nonnull self, Script* _Nonnull script, ExecuteOptions options)
{
    decl_try_err();
    const bool bPushScope = (options & kExecute_PushScope) == kExecute_PushScope;
    const bool bInteractive = (options & kExecute_Interactive) == kExecute_Interactive;

    //Script_Print(script);
    //putchar('\n');

    if (bPushScope) {
        RunStack_PushScope(self->runStack);
    }

    self->isInteractive = bInteractive;
    err = Interpreter_ExpressionList(self, &script->exprs, bInteractive);

    if (bPushScope) {
        RunStack_PopScope(self->runStack);
    }

    OpStack_PopAll(self->opStack);
    StackAllocator_DeallocAll(self->allocator);
    return err;
}
