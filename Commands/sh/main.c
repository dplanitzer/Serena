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


void main_closure(int argc, char *argv[])
{
    // XXX disabled insertion mode for now because the line reader doesn't support
    // XXX it properly yet
    //printf("\033[4h");  // Switch the console to insert mode
    printf("Apollo v0.1.\nCopyright 2023, Dietmar Planitzer.\n\n");

    mkdir("/Users", 0755);
    mkdir("/Users/Admin", 0755);
    mkdir("/Users/Tester", 0755);


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

        putchar('\n');
        
        Parser_Parse(pParser, line, pScript);
        Interpreter_Execute(pInterpreter, pScript);
    }
}
