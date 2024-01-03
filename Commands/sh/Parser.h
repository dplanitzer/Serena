//
//  Parser.h
//  sh
//
//  Created by Dietmar Planitzer on 1/2/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Lexer.h"


typedef struct _Script {
    char* _Nonnull * _Nonnull   words;
    int                         wordCount;
    int                         wordCapacity;
} Script;
typedef Script* ScriptRef;

extern errno_t Script_Create(ScriptRef _Nullable * _Nonnull pOutScript);
extern void Script_Destroy(ScriptRef _Nullable self);
extern void Script_Print(ScriptRef _Nonnull self);


typedef struct _Parser {
    Lexer                           lexer;
    ScriptRef _Nullable /* weak */  script;
} Parser;
typedef Parser* ParserRef;


extern errno_t Parser_Create(ParserRef _Nullable * _Nonnull pOutParser);
extern void Parser_Destroy(ParserRef _Nullable self);

// Parses the text 'text' and updates the script object 'pScript' to reflect the
// result of parsing 'text'.
extern void Parser_Parse(ParserRef _Nonnull self, const char* _Nonnull text, ScriptRef _Nonnull pScript);
