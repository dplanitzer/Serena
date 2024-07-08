//
//  Interpreter.c
//  sh
//
//  Created by Dietmar Planitzer on 1/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Errors.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <System/System.h>

extern int cmd_cd(ShellContext* _Nonnull pContext, int argc, char** argv);
extern int cmd_cls(ShellContext* _Nonnull pContext, int argc, char** argv);
extern int cmd_delete(ShellContext* _Nonnull pContext, int argc, char** argv);
extern int cmd_echo(ShellContext* _Nonnull pContext, int argc, char** argv);
extern int cmd_exit(ShellContext* _Nonnull pContext, int argc, char** argv);
extern int cmd_history(ShellContext* _Nonnull pContext, int argc, char** argv);
extern int cmd_list(ShellContext* _Nonnull pContext, int argc, char** argv);
extern int cmd_makedir(ShellContext* _Nonnull pContext, int argc, char** argv);
extern int cmd_pwd(ShellContext* _Nonnull pContext, int argc, char** argv);
extern int cmd_rename(ShellContext* _Nonnull pContext, int argc, char** argv);
extern int cmd_type(ShellContext* _Nonnull pContext, int argc, char** argv);

static errno_t Interpreter_RegisterGlobalSymbols(InterpreterRef _Nonnull self);

////////////////////////////////////////////////////////////////////////////////

errno_t Interpreter_Create(ShellContextRef _Nonnull pContext, InterpreterRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    InterpreterRef self;
    
    try_null(self, calloc(1, sizeof(Interpreter)), ENOMEM);
    try(StackAllocator_Create(1024, 8192, &self->allocator));
    self->context = pContext;
    try(SymbolTable_Create(&self->symbolTable));
    try(Interpreter_RegisterGlobalSymbols(self));

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
        SymbolTable_Destroy(self->symbolTable);
        self->symbolTable = NULL;

        StackAllocator_Destroy(self->allocator);
        self->allocator = NULL;

        self->context = NULL;
        free(self);
    }
}

static errno_t Interpreter_RegisterGlobalSymbols(InterpreterRef _Nonnull self)
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

static bool Interpreter_ExecuteInternalCommand(InterpreterRef _Nonnull self, int argc, char** argv)
{
    Symbol* cmd = SymbolTable_GetSymbol(self->symbolTable, kSymbolType_Command, argv[0]);

    if (cmd) {
        cmd->u.command.cb(self->context, argc, argv);
        return true;
    }

    return false;
}

static bool Interpreter_ExecuteExternalCommand(InterpreterRef _Nonnull self, int argc, char** argv)
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

    err = Process_Spawn(cmdPath, argv, &opts, &childPid);
    if (err == ENOENT) {
        return false;
    }
    else if (err != EOK) {
        printf("%s: %s.\n", argv[0], shell_strerror(err));
        return true;
    }

    ProcessTerminationStatus pts;
    Process_WaitForTerminationOfChild(childPid, &pts);
    
    return true;
}

static int calc_argc(SExpression* _Nonnull s_expr)
{
    Atom* atom = s_expr->atoms;
    int argc = 0;

    if (atom) {
        atom = atom->next;
        argc++;
    }

    while (atom) {
        if (atom->hasLeadingWhitespace) {
            argc++;
        }

        atom = atom->next;
    }

    return argc;
}

static size_t calc_atom_string_length(Atom* _Nonnull atom)
{
    switch (atom->type) {
        case kAtom_Character:
        case kAtom_Assignment:
        case kAtom_Plus:
        case kAtom_Minus:
        case kAtom_Multiply:
        case kAtom_Divide:
        case kAtom_Less:
        case kAtom_Greater:
            return 1;

        case kAtom_LessEqual:
        case kAtom_GreaterEqual:
        case kAtom_NotEqual:
        case kAtom_Equal:
            return 2;

        case kAtom_UnquotedString:
        case kAtom_SingleQuotedString:
        case kAtom_DoubleQuotedString:
        case kAtom_EscapedCharacter:
            return atom->u.string.length;

        default:
            return 0;
    }
}

static char* _Nonnull get_atom_string(Atom* _Nonnull atom, char* _Nonnull ap)
{
    switch (atom->type) {
        case kAtom_Character:           *ap++ = atom->u.character; break;
        case kAtom_Assignment:          *ap++ = '='; break;
        case kAtom_Plus:                *ap++ = '+'; break;
        case kAtom_Minus:               *ap++ = '-'; break;
        case kAtom_Multiply:            *ap++ = '*'; break;
        case kAtom_Divide:              *ap++ = '/'; break;
        case kAtom_Less:                *ap++ = '<'; break;
        case kAtom_Greater:             *ap++ = '>'; break;

        case kAtom_LessEqual:           *ap++ = '<'; *ap++ = '='; break;
        case kAtom_GreaterEqual:        *ap++ = '>'; *ap++ = '='; break;
        case kAtom_NotEqual:            *ap++ = '!'; *ap++ = '='; break;
        case kAtom_Equal:               *ap++ = '='; *ap++ = '='; break;

        case kAtom_UnquotedString:
        case kAtom_SingleQuotedString:
        case kAtom_DoubleQuotedString:
        case kAtom_EscapedCharacter:
            memcpy(ap, atom->u.string.chars, atom->u.string.length * sizeof(char));
            ap += atom->u.string.length;
            break;

        default:
            break;
    }

    return ap;
}

static void Interpreter_SExpression(InterpreterRef _Nonnull self, SExpression* _Nonnull s_expr)
{
    decl_try_err();
    char** argv = NULL;

    // Create the command argument vector by expanding all atoms in the s-expression
    // to strings.
    int argc = calc_argc(s_expr);
    if (argc == 0) {
        return;
    }

    try_null(argv, StackAllocator_Alloc(self->allocator, sizeof(char*) * (argc + 1)), ENOMEM);

    argc = 0;
    Atom* atom = s_expr->atoms;
    while (atom) {
        Atom* sav_atom = atom;
        Atom* end_atom;

        // We always pick up the first atom in an non-whitespace-separated-atom-sequence
        // The 2nd, 3rd, etc we only pick up if they don't have leading whitespace
        size_t arg_size = calc_atom_string_length(atom);
        atom = atom->next;
        while (atom && !atom->hasLeadingWhitespace) {
            arg_size += calc_atom_string_length(atom);
            atom = atom->next;
        }
        end_atom = atom;


        if (arg_size > 0) {
            char* ap = NULL;
            char* cap;

            try_null(ap, StackAllocator_Alloc(self->allocator, sizeof(char) * (arg_size + 1)), ENOMEM);

            cap = ap;
            atom = sav_atom;
            while (atom != end_atom) {
                cap = get_atom_string(atom, cap);
                atom = atom->next;
            }
            *cap = '\0';

            argv[argc++] = ap;
        }
    }
    argv[argc] = NULL;

    if (argc == 0) {
        return;
    }

    // Check whether this is a builtin command and execute it, if so
    if (Interpreter_ExecuteInternalCommand(self, argc, argv)) {
        return;
    }


    // Not a builtin command. Look for an external command
    if (Interpreter_ExecuteExternalCommand(self, argc, argv)) {
        return;
    }


    // Not a command at all
    printf("%s: unknown command.\n", argv[0]);
    return;

catch:
    printf(shell_strerror(err));
}

static void Interpreter_PExpression(InterpreterRef _Nonnull self, PExpression* _Nonnull p_expr)
{
    SExpression* s_expr = p_expr->exprs;

    // XXX create an intermediate representation that allows us to model a set of
    // XXX commands that are linked through pipes. For now we'll do each command
    // XXX individually. Which is wrong. But good enough for now
    while (s_expr) {
        Interpreter_SExpression(self, s_expr);
        s_expr = s_expr->next;
    }
}

static void Interpreter_Block(InterpreterRef _Nonnull self, Block* _Nonnull block)
{
    PExpression* p_expr = block->exprs;

    while (p_expr) {
        Interpreter_PExpression(self, p_expr);
        p_expr = p_expr->next;
    }
}

// Interprets 'script' and executes all its statements.
void Interpreter_Execute(InterpreterRef _Nonnull self, Script* _Nonnull script)
{
    //Script_Print(script);
    //putchar('\n');

    Interpreter_Block(self, script->body);
    StackAllocator_DeallocAll(self->allocator);
}
