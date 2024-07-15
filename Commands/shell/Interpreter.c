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

static errno_t Interpreter_RegisterBuiltinCommand(InterpreterRef _Nonnull self);
static errno_t Interpreter_RegisterEnvironmentVariables(InterpreterRef _Nonnull self);


////////////////////////////////////////////////////////////////////////////////

errno_t Interpreter_Create(ShellContextRef _Nonnull pContext, InterpreterRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    InterpreterRef self;
    
    try_null(self, calloc(1, sizeof(Interpreter)), ENOMEM);
    try(StackAllocator_Create(1024, 8192, &self->allocator));
    self->context = pContext;
    try(SymbolTable_Create(&self->symbolTable));
    try(Interpreter_RegisterBuiltinCommand(self));
    try(Interpreter_RegisterEnvironmentVariables(self));
    try(EnvironCache_Create(self->symbolTable, &self->environCache));
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

        SymbolTable_Destroy(self->symbolTable);
        self->symbolTable = NULL;

        StackAllocator_Destroy(self->allocator);
        self->allocator = NULL;

        self->context = NULL;
        free(self);
    }
}

static errno_t Interpreter_RegisterBuiltinCommand(InterpreterRef _Nonnull self)
{
    decl_try_err();

    try(SymbolTable_AddCommand(self->symbolTable, "cd", cmd_cd));
    try(SymbolTable_AddCommand(self->symbolTable, "cls", cmd_cls));
    try(SymbolTable_AddCommand(self->symbolTable, "delete", cmd_delete));
    try(SymbolTable_AddCommand(self->symbolTable, "echo", cmd_echo));
    try(SymbolTable_AddCommand(self->symbolTable, "exit", cmd_exit));
    try(SymbolTable_AddCommand(self->symbolTable, "history", cmd_history));
    try(SymbolTable_AddCommand(self->symbolTable, "list", cmd_list));
    try(SymbolTable_AddCommand(self->symbolTable, "makedir", cmd_makedir));
    try(SymbolTable_AddCommand(self->symbolTable, "pwd", cmd_pwd));
    try(SymbolTable_AddCommand(self->symbolTable, "rename", cmd_rename));
    try(SymbolTable_AddCommand(self->symbolTable, "type", cmd_type));

catch:
    return err;
}

static errno_t Interpreter_RegisterEnvironmentVariables(InterpreterRef _Nonnull self)
{
    decl_try_err();
    ProcessArguments* pargs = Process_GetArguments();
    char** envp = pargs->envp;

    while (*envp) {
        char* keyp = *envp;
        char* eqp = strchr(keyp, '=');
        char* valp = (eqp) ? eqp + 1 : NULL;

        if (valp) {
            *eqp = '\0';
            err = SymbolTable_AddVariable(self->symbolTable, keyp, valp, kVariableFlag_Exported);
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
    Symbol* cmd = SymbolTable_GetSymbol(self->symbolTable, kSymbolType_Command, argv[0]);

    if (cmd) {
        cmd->u.command.cb(self->context, argc, argv, envp);
        return true;
    }

    return false;
}

static bool Interpreter_ExecuteExternalCommand(InterpreterRef _Nonnull self, int argc, char** argv, char** envp)
{
    static const char* gSearchPath = "/System/Commands/";
    decl_try_err();
    ProcessId childPid;
    SpawnOptions opts = {0};
    
    const size_t cmdPathLen = strlen(gSearchPath) + strlen(argv[0]);
    if (cmdPathLen > PATH_MAX) {
        printf("Command line too long.\n");
        return true;
    }

    char* cmdPath = StackAllocator_Alloc(self->allocator, sizeof(char*) * (cmdPathLen + 1));
    if (cmdPath == NULL) {
        printf(shell_strerror(ENOMEM));
        return true;
    }

    strcpy(cmdPath, gSearchPath);
    strcat(cmdPath, argv[0]);

    opts.envp = envp;


    // Spawn the external command
    err = Process_Spawn(cmdPath, argv, &opts, &childPid);
    if (err == ENOENT) {
        return false;
    }
    else if (err != EOK) {
        printf("%s: %s.\n", argv[0], shell_strerror(err));
        return true;
    }


    // Wait for the command to complete its task
    ProcessTerminationStatus pts;
    Process_WaitForTerminationOfChild(childPid, &pts);
    
    return true;
}

static errno_t Interpreter_WriteQuotedStringText(InterpreterRef _Nonnull self, QuotedString* _Nonnull str)
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
            case kStringAtom_VariableReference:
                break;

            default:
                abort();
                break;
        }
        atom = atom->next;
    }
    return err;
}

static errno_t Interpreter_WriteArgumentString(InterpreterRef _Nonnull self, Atom* _Nonnull atom)
{
    decl_try_err();

    switch (atom->type) {
        case kAtom_Expression:
        case kAtom_VariableReference:
            break;

        case kAtom_QuotedString:
            err = Interpreter_WriteQuotedStringText(self, atom->u.qstring);
            break;

        default:
            err = ArgumentVector_AppendBytes(self->argumentVector, Atom_GetString(atom), Atom_GetStringLength(atom));
            break;
    }

    return err;
}

static void Interpreter_Command(InterpreterRef _Nonnull self, Command* _Nonnull cmd)
{
    decl_try_err();

    // Create the command argument vector by converting all atoms in the command
    // expression into argument strings.
    ArgumentVector_Open(self->argumentVector);

    Atom* atom = cmd->atoms;
    while (atom) {
        bool isFirstAtom = true;

        // We always pick up the first atom in an non-whitespace-separated-atom-sequence
        // The 2nd, 3rd, etc we only pick up if they don't have leading whitespace
        while (atom && (!atom->hasLeadingWhitespace || isFirstAtom)) {
            try(Interpreter_WriteArgumentString(self, atom));
            atom = atom->next;
            isFirstAtom = false;
        }

        try(ArgumentVector_EndOfArg(self->argumentVector));
    }
    try(ArgumentVector_Close(self->argumentVector));
    
    const int argc = ArgumentVector_GetArgc(self->argumentVector);
    char** argv = ArgumentVector_GetArgv(self->argumentVector);
    if (argc == 0) {
        return;
    }

    char** envp = EnvironCache_GetEnvironment(self->environCache);


    // Check whether this is a builtin command and execute it, if so
    if (Interpreter_ExecuteInternalCommand(self, argc, argv, envp)) {
        return;
    }


    // Not a builtin command. Look for an external command
    if (Interpreter_ExecuteExternalCommand(self, argc, argv, envp)) {
        return;
    }


    // Not a command at all
    printf("%s: unknown command.\n", argv[0]);
    return;

catch:
    printf(shell_strerror(err));
}

static void Interpreter_Expression(InterpreterRef _Nonnull self, Expression* _Nonnull expr)
{
    Command* cmd = expr->cmds;

    // XXX create an intermediate representation that allows us to model a set of
    // XXX commands that are linked through pipes. For now we'll do each command
    // XXX individually. Which is wrong. But good enough for now
    while (cmd) {
        Interpreter_Command(self, cmd);
        cmd = cmd->next;
    }
}

static void Interpreter_Statement(InterpreterRef _Nonnull self, Statement* _Nonnull stmt)
{
    switch (stmt->type) {
        case kStatementType_Null:
            break;

        case kStatementType_Expression:
            Interpreter_Expression(self, stmt->u.expr);
            break;

        case kStatementType_VarAssignment:
            break;

        case kStatementType_VarDeclaration:
            break;

        default:
            abort();
            break;
    }
}

static void Interpreter_StatementList(InterpreterRef _Nonnull self, StatementList* _Nonnull stmts)
{
    Statement* stmt = stmts->stmts;

    while (stmt) {
        Interpreter_Statement(self, stmt);
        stmt = stmt->next;
    }
}

static void Interpreter_Block(InterpreterRef _Nonnull self, Block* _Nonnull block)
{
    Interpreter_StatementList(self, &block->statements);
}

// Interprets 'script' and executes all its statements.
void Interpreter_Execute(InterpreterRef _Nonnull self, Script* _Nonnull script)
{
    //Script_Print(script);
    //putchar('\n');

    Interpreter_StatementList(self, &script->statements);
    StackAllocator_DeallocAll(self->allocator);
}
