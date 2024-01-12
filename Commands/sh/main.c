//
//  main.c
//  sh
//
//  Created by Dietmar Planitzer on 12/29/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "Interpreter.h"
#include "LineReader.h"
#include "Parser.h"

extern void _mkdir(const char*);


void main_closure(int argc, char *argv[])
{
//    assert(open("/dev/console", O_RDONLY) == 0);
//    assert(open("/dev/console", O_WRONLY) == 1);
    int fd0 = open("/dev/console", O_RDONLY);
    int fd1 = open("/dev/console", O_WRONLY);

    printf("\033[4h");  // Switch the console to insert mode
    printf("Apollo v0.1.\nCopyright 2023, Dietmar Planitzer.\n\n");

    _mkdir("/Users");
    _mkdir("/Users/Admin");
    _mkdir("/Users/Tester");


    LineReaderRef pLineReader = NULL;
    ScriptRef pScript = NULL;
    ParserRef pParser = NULL;
    InterpreterRef pInterpreter = NULL;

    LineReader_Create(79, 10, ">", &pLineReader);
    Script_Create(&pScript);
    Parser_Create(&pParser);
    Interpreter_Create(&pInterpreter);

    while (true) {
        char* line = LineReader_ReadLine(pLineReader);

        puts("");
        
        Parser_Parse(pParser, line, pScript);
        Interpreter_Execute(pInterpreter, pScript);
    }
}
