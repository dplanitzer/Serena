# Expressions

Every line in a Serena shell script is an expressions. Whether the purpose of a line is to invoke a command, do some mathematical calculations, control the flow of execution or declare a variable. Each and all of those lines are treated as expressions that do some work and produce a value.

The value of an expression is printed to the terminal if the expression is evaluated in interactive mode. Meaning, you entered the expression on the command prompt and hit return. On the other hand, if an expression is executed in the context of a script then the value of the expression is not automatically printed. Instead the value of the expression is either assigned to a variable or discarded.

Note that the value of an expression may be the special value "void". This value signals that an expression did not produce an integer, string or boolean value and that the result of the expression should be ignored.

Expressions can be broken done into two big subgroups:

* Commands
* Mathematical expressions

The following sections describe each kind of expressions and how the shell distinguishes between them.

Each expression must be terminated by an expression terminator. Supported expression terminators are:

* the newline character
* a semicolon
* an ampersand

The difference between an ampersand and the other terminators is that an ampersand causes the shell to execute the expression asynchronously while the other terminators cause it to execute the expression synchronously.

## Commands

Commands do things. They may create a file or directory, rename them, move them, delete them or do other things with them.

There are internal and external commands. An internal command is built right into the shell and thus the shell does not have to spend time on loading such a command when you invoke it. An external command is an executable that is stored on disk and that is invoked by using its name. The shell maintains a search path that is an array of paths and it uses this search path to find an external command. That way it is sufficient to write just the name of an external command to execute it.

A command may take parameters. How many parameters it accepts and what the meaning of each of them is is determined by each command individually. The shell simply treats all text that follows a command name, up until it encounters an expression terminator as a parameter to the command.

The way that the shell detects that an expression is in fact a command is by looking at the first chunk of text that is surrounded by whitespace: if this text is an identifier rather than a string, integer or boolean literal, then the expression is treated as a command.

## Mathematical Expressions

A mathematical expression calculates some result that is made available to the surrounding context as a typed value. An expression may produce an integer, string or boolean value.

Here the term mathematical expression is understood in a wider context: it includes arithmetic, but also comparisons, logical operations and string concatenations. See the section on [Operators](Operators.md) for a complete list of supported operators.

## Combining Commands and Expressions

You can freely combine commands and expressions by embedding an expression in a command invocation. You do this by enclosing the expression in parenthesis like this:

```
> echo (1 + 2)

3
```

The shell first evaluates all parenthesized expressions to calculate their values and it then converts those values to strings before it executes the command.

Adjacent non-whitespace separated strings are automatically concatenated into a single contiguous string before command execution. This is even the case for variables and embedded expressions:

```
> echo abc"$HOME"def abc(1+2)def

abc/Users/Administratordef abc3def
```

Here the shell first evaluates the variable reference '$HOME' and the embedded expression '1+2'. It then concatenates the strings 'abc', 'def' and the string value of the variable '$HOME' to generate a single argument string for the command because all 3 strings are directly adjacent to each other (not whitespace separated). It does the same with the result of the '1+2' expression: it concatenates 'abc', 'def' and the string '3' to generate the second argument to the command.
