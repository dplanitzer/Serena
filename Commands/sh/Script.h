//
//  Script.h
//  sh
//
//  Created by Dietmar Planitzer on 1/2/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Script_h
#define Script_h

#include <apollo/apollo.h>

struct Statement;
typedef struct Statement* StatementRef;


typedef struct Statement {
    StatementRef _Nullable      next;
    char* _Nonnull * _Nonnull   words;
    int                         wordCount;
    int                         wordCapacity;
} Statement;

extern errno_t Statement_Create(StatementRef _Nullable * _Nonnull pOutStatement);
extern void Statement_Destroy(StatementRef _Nullable self);
extern void Statement_Print(StatementRef _Nonnull self);
extern errno_t Statement_AddWord(StatementRef _Nonnull self, const char* _Nonnull pWord);



typedef struct Script {
    StatementRef _Nullable  statements;
    StatementRef _Nullable  lastStatement;
} Script;
typedef Script* ScriptRef;

extern errno_t Script_Create(ScriptRef _Nullable * _Nonnull pOutScript);
extern void Script_Destroy(ScriptRef _Nullable self);
extern void Script_Reset(ScriptRef _Nonnull self);
extern void Script_Print(ScriptRef _Nonnull self);
extern void Script_AddStatement(ScriptRef _Nonnull self, StatementRef _Nonnull pStatement);

#endif  /* Script_h */