//
//  uptime.c
//  sh
//
//  Created by Dietmar Planitzer on 8/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Utilities.h"
#include <stdlib.h>
#include <clap.h>

struct Context {
    bool    matchPublic;
    size_t  outVarCount;
};

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("vars")
);


static errno_t iter_vars(struct Context* _Nonnull ctx, const Variable* _Nonnull v, int level, bool* _Nonnull pOutDone)
{
    if (ctx->matchPublic && (v->modifiers & kVarModifier_Public) == 0) {
        return EOK;
    }
    if (!ctx->matchPublic && (v->modifiers & kVarModifier_Public) == kVarModifier_Public) {
        return EOK;
    }

    fputs(v->name, stdout);
    fputc('=', stdout);
    Value_Write(&v->value, stdout);
    fputc('\n', stdout);

    ctx->outVarCount++;

    return EOK;
}

static errno_t do_vars(InterpreterRef _Nonnull ip)
{
    struct Context ctx;

    // Internal vars
    ctx.matchPublic = false;
    ctx.outVarCount = 0;
    Interpreter_IterateVariables(ip, (RunStackIterator)iter_vars, &ctx);

    if (ctx.outVarCount > 0) {
        puts("");
    }

    // Public vars
    ctx.matchPublic = true;
    ctx.outVarCount = 0;
    Interpreter_IterateVariables(ip, (RunStackIterator)iter_vars, &ctx);

    return EOK;
}

int cmd_vars(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int exitCode;

    if (!clap_should_exit(status)) {
        exitCode = exit_code(do_vars(ip));
    }
    else {
        exitCode = clap_exit_code(status);
    }
    OpStack_PushVoid(ip->opStack);
    
    return exitCode;
}
