//
//  Parser.h
//  sh
//
//  Created by Dietmar Planitzer on 1/2/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Parser_h
#define Parser_h

#include "Lexer.h"
#include "Script.h"

typedef struct _Parser {
    Lexer                                   lexer;
    StackAllocatorRef _Nullable /*Weak*/    scriptAllocator;
} Parser;
typedef Parser* ParserRef;


extern errno_t Parser_Create(ParserRef _Nullable * _Nonnull pOutSelf);
extern void Parser_Destroy(ParserRef _Nullable self);

// Parses the text 'text' and updates the script object 'pScript' to reflect the
// result of parsing 'text'.
extern void Parser_Parse(ParserRef _Nonnull self, const char* _Nonnull text, ScriptRef _Nonnull pScript);

#endif  /* Parser_h */
