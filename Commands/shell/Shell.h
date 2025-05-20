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

typedef struct Shell {
    LineReaderRef _Nullable     lineReader;
    Parser* _Nonnull            parser;
    InterpreterRef _Nonnull     interpreter;
} Shell;
typedef Shell* ShellRef;


// Creates a shell instance. The shell will be interactive if 'isInteractive' is
// true. Otherwise the shell will not present a user interface and instead you
// should use RunContentsOfFile commands to execute shell scripts. 
extern ShellRef _Nonnull Shell_Create(bool isInteractive);

// Destroys the given shell instance.

extern void Shell_Destroy(ShellRef _Nullable self);

// Runs the main loop of an interactive shell. The shell uses a line reader to
// get the input from the user and it then parses and interprets the input before
// it loops around to get the next line. Returns an error if ie the main loop
// encounters an out-of-memory error. Otherwise blocks until the user explicitly
// exits the interactive shell.
extern errno_t Shell_Run(ShellRef _Nonnull self);

// Interprets the given string as a command or series of commands.
extern errno_t Shell_RunContentsOfString(ShellRef _Nonnull self, const char* _Nonnull str);

// Interprets the contents of the given file as a shell script and runs the commands
// in that script, line by line.
extern errno_t Shell_RunContentsOfFile(ShellRef _Nonnull self, const char* _Nonnull path);

#endif  /* Shell_h */
