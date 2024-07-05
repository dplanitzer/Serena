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

#define ESYNTAX -1


typedef struct Parser {
    Lexer                                   lexer;
    StackAllocatorRef _Nullable /*Weak*/    allocator;
} Parser;


extern errno_t Parser_Create(Parser* _Nullable * _Nonnull pOutSelf);
extern void Parser_Destroy(Parser* _Nullable self);

// Parses the text 'text' and updates the script object 'script' to reflect the
// result of parsing 'text'.
extern errno_t Parser_Parse(Parser* _Nonnull self, const char* _Nonnull text, Script* _Nonnull script);

#endif  /* Parser_h */
