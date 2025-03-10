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

struct Interpreter;

//
// Input:
// - serialized arguments
// - serialized environment variables
//
// Output:
// - one value on the op-stack (corresponding to a captured stdout)
// - status
//

extern int cmd_cd(struct Interpreter* _Nonnull ip, int argc, char** argv, char** envp);
extern int cmd_cls(struct Interpreter* _Nonnull ip, int argc, char** argv, char** envp);
extern int cmd_delete(struct Interpreter* _Nonnull ip, int argc, char** argv, char** envp);
extern int cmd_delay(struct Interpreter* _Nonnull ip, int argc, char** argv, char** envp);
extern int cmd_echo(struct Interpreter* _Nonnull ip, int argc, char** argv, char** envp);
extern int cmd_exists(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp);
extern int cmd_exit(struct Interpreter* _Nonnull ip, int argc, char** argv, char** envp);
extern int cmd_history(struct Interpreter* _Nonnull ip, int argc, char** argv, char** envp);
extern int cmd_id(struct Interpreter* _Nonnull ip, int argc, char** argv, char** envp);
extern int cmd_input(struct Interpreter* _Nonnull ip, int argc, char** argv, char** envp);
extern int cmd_list(struct Interpreter* _Nonnull ip, int argc, char** argv, char** envp);
extern int cmd_load(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp);
extern int cmd_makedir(struct Interpreter* _Nonnull ip, int argc, char** argv, char** envp);
extern int cmd_pwd(struct Interpreter* _Nonnull ip, int argc, char** argv, char** envp);
extern int cmd_rename(struct Interpreter* _Nonnull ip, int argc, char** argv, char** envp);
extern int cmd_save(struct Interpreter* _Nonnull ip, int argc, char** argv, char** envp);
extern int cmd_shutdown(struct Interpreter* _Nonnull ip, int argc, char** argv, char** envp);
extern int cmd_uptime(struct Interpreter* _Nonnull ip, int argc, char** argv, char** envp);
extern int cmd_vars(struct Interpreter* _Nonnull ip, int argc, char** argv, char** envp);

#endif  /* Builtins_h */
