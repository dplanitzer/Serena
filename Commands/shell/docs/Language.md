# The Serena Shell Language

The Serena shell language is a formally defined language which is fully expression based. It supports expressions, scoped variables, flow control, builtin commands and external commands. It is more similar in design to shell languages like PowerShell or NuShell than it is to bash or csh.

_Note:_ The current version of the shell only implements a small subset of the language described below.

## Language Basics

A shell script is a sequence of expressions. An expression in turn is either a mathematical expression, a single command or a pipeline of multiple commands. The end of an expression is marked by a newline, semicolon or an ampersand. A newline or a semicolon at the end of an expression causes the shell to evaluate the expression synchronously. This means that the shell waits for the expression to finish evaluation before it starts evaluating the next expression. An expression that ends in an ampersand on the other hand causes the shell to evaluate the expression asynchronously. Meaning, the shell starts evaluating the next expression even before the current one has finished its evaluation.

A pipeline is a series of commands that are linked together in the sense that the output of one command is fed as input into the next command. Note that the first stage of a pipeline may be a mathematical expression instead.

Use the '#' character to start a comment. The shell ignores every character between the '#' and the next newline character.

The following is a simple example of a shell script:

```
# This is a comment
echo -n "The Serena Shell Language"
echo " is nice!"

makedir --parents a b c d e f
shell my-shell-script.sh

list "foo bar"; list other_dir
```

You run a script like this by typing 'shell' followed by the name of the shell script.

### Escaped Characters

Escape a character on the command line by prefixing it with a '\\' (backslash) character. Escaping a character prevents the shell from interpreting the character and it is instead treated as a literal character with no special meaning attached to it.

For example, the character sequence '\\+' causes the shell to insert the literal character '+' into the command line and the character sequence '\\$' prevents the shell from interpreting the \$ as the beginning of a variable reference. Instead \$ is inserted into the command line as the literal \$ character.

### Single Quoted Strings

A single quoted string is a string that is enclosed in ' (single quote) characters. The shell does not interpret any of the characters between single quotes.

For example, the string

```
'Hello "World"!'
```

produces the result

```
Hello "World"!
```

### Double Quoted Strings

A double quoted string is a string that is enclosed in " (double quote) characters. Most of the characters inside such a string are not interpreted by the shell. However there are some exceptions:

* \$ Marks the beginning of a variable reference
* \\ Marks the beginning of an escape sequence

A double quoted string may span more than one line by escaping the trailing newline in a line with a \ (backslash) character. For example the string:

```
"Hello \
World"
```

would be printed as

```
Hello World
```

### Escape Sequences

An escape sequence is a special sequence of characters that triggers some predetermined function and they are only recognized inside of a double quoted string. Escape sequences are based on the same sequences supported in ANSI C plus some additional sequences. The supported sequences are:

* \\a Plays the bell sound
* \\b Inserts a backspace character
* \\e Inserts an escape character
* \\f Inserts a form feed character
* \\v Inserts a vertical tabulator
* \\r Inserts a carriage return character
* \\n Inserts a linefeed character
* \\$ Inserts a \$ character
* \\" Inserts a double quote
* \\' Inserts a single quote
* \\\\ Inserts a backslash
* \\ddd Inserts a character calculated from the provided octal value. There may be 1, 2 or 3 digits
* \\xdd or \\Xdd Inserts a character calculated from the provided hexadecimal value. There may be one or two digits
* \\NL This is a backslash followed by a literal newline and is used to escape a newline which allows you to continue a string on the next line

Note that all other characters following a backslash are reserved.
