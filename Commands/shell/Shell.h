//
//  Shell.h
//  sh
//
//  Created by Dietmar Planitzer on 2/25/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Shell_h
#define Shell_h

#include "LineReader.h"
#include "Interpreter.h"
#include "Parser.h"
#include "ShellContext.h"

typedef struct Shell {
    LineReaderRef _Nullable     lineReader;
    ShellContextRef _Nonnull    context;
    ParserRef _Nonnull          parser;
    InterpreterRef _Nonnull     interpreter;
} Shell;
typedef Shell* ShellRef;


// Creates an instance of an interactive shell.
extern errno_t Shell_CreateInteractive(ShellRef _Nullable * _Nonnull pOutSelf);

// Destroys the given shell instance.

extern void Shell_Destroy(ShellRef _Nullable self);

// Runs the main loop of an interactive shell. The shell uses a line reader to
// get the input from the user and it then parses and interprets the input before
// it loops around to get the next line. Returns an error if ie the main loop
// encounters an out-of-memory error. Otherwise blocks until the user explicitly
// exits the interactive shell.
extern errno_t Shell_Run(ShellRef _Nonnull self);

#endif  /* Shell_h */
