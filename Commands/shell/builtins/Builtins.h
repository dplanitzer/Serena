//
//  Builtins.h
//  sh
//
//  Created by Dietmar Planitzer on 7/9/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Builtins_h
#define Builtins_h

#include <System/_cmndef.h>

struct ShellContext;

extern int cmd_cd(struct ShellContext* _Nonnull ctx, int argc, char** argv, char** envp);
extern int cmd_cls(struct ShellContext* _Nonnull ctx, int argc, char** argv, char** envp);
extern int cmd_delete(struct ShellContext* _Nonnull ctx, int argc, char** argv, char** envp);
extern int cmd_echo(struct ShellContext* _Nonnull ctx, int argc, char** argv, char** envp);
extern int cmd_exit(struct ShellContext* _Nonnull ctx, int argc, char** argv, char** envp);
extern int cmd_history(struct ShellContext* _Nonnull ctx, int argc, char** argv, char** envp);
extern int cmd_list(struct ShellContext* _Nonnull ctx, int argc, char** argv, char** envp);
extern int cmd_makedir(struct ShellContext* _Nonnull ctx, int argc, char** argv, char** envp);
extern int cmd_pwd(struct ShellContext* _Nonnull ctx, int argc, char** argv, char** envp);
extern int cmd_rename(struct ShellContext* _Nonnull ctx, int argc, char** argv, char** envp);
extern int cmd_type(struct ShellContext* _Nonnull ctx, int argc, char** argv, char** envp);

#endif  /* Builtins_h */
