# The Shell

The shell enables you to launch executables, navigate the file system, list the contents of directories, work with files and other things. A number of commands are built directly into the shell. You can find a description of each command in the list below.

## Line Editor

You enter shell commands in the shell's line editor. The line editor allows you to enter, edit and recall previously entered commands. Please note that the line editor is currently limited to override/replace mode.

To enter a command, type it in the currently active line and hit the return/enter key. This triggers the execution of the command and it adds the command to the command history. A previously entered command may be recalled by pressing the cursor-up key. You can scroll through all commands in the command history by pressing the cursor-up and cursor-down keys.

Some simple editing functions are supported. Backspace moves the cursor one column to the left, the cursor-left and cursor-right keys move the cursor left and right respectively. Ctrl-a moves the cursor to the beginning of the line and Ctrl-e moves the cursor to the end of the line. Finally, pressing Ctrl-l clears the screen.

## The Shell Language

The shell language supports commands and expressions and its design is somewhat influenced by PowerShell, Nushell, AmigaDOS and Bash. A formal definition can be found [here](Language.md).

### Language Basics

A shell script is a sequence of commands. A command consists of the name of the command plus a whitespace separated list of command arguments. The end of a command is marked by a semicolon, ampersand or just a sigle newline character. A semicolon at the end of a command causes the shell to wait for the command to finish execution before it executes the next command while an ampersand at the end of a command causes the shell to start executing the next command before the current one has completed.

Use the '#' character to start a comment. The shell ignores every character between the '#' and the next newline character.

The following is a simple example of a shell script:

```
# This is a comment
echo -n "The Serena Shell Language"
echo " is nice!"

makedir --parents a b c d e f
shell my-shell-script.sh

list "foo bar"; list "a\$b"
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

### Commands

#### CD \<path>

Set the current working directory to the directory indicated by the provided path.

#### CLS

Clears the screen.

#### DELETE \<path ...>

Deletes one or more directories or files indicated by the provided path(s). A directory must be empty to be eligible for deleting.

#### DELAY \<ms>

Pauses execution for 'ms' milliseconds.

#### ECHO [-n | --noline] [-s | --nospace] \<strings ...>

Prints the given list of strings separated by single space characters to standard out. The list of strings is followed by a single newline character. The output of the final newline character may be suppressed by specifying the '--noline' option. Use the '--nospace' option to print multiple strings without a separating space character. All strings following a '--' parameter are printed verbatim and not interpreted as an option even if they start with '-' or '--' characters.

#### EXIT [exit_code]

Exits the current shell with the exit code 'exit_code'. The exit code is passed to the program that invoked the shell. An exit code of 0 is assumed if no exit code is provided.

#### HISTORY

Prints the contents of the shell history to standard out. Each history entry is printed as a separate line. The entries are printed in the order of newest to oldest.

#### LIST [-a | --all] [paths ...]

Lists the contents of one or more directories as indicated by the provided paths. Lists the contents of the current working directory if no path is provided. By default the list command does not list entries that start with a '.'. Pass the '--all' parameter to enable the output of all directory entries.

#### MAKEDIR [-p | --parents] \<path ...>

Creates one or more empty directories at the file system locations designated by the provided paths. The last path component in a path specifies the name of the directory to create. Any non-existing intermediate directories are automatically created if you pass the '--parents' option.

#### PWD

Prints the absolute path of the current working directory.

#### RENAME \<source_path> \<destination_path>

Renames the file or directory at 'source_path' to 'destination_path'. If a file or directory exists at 'destination_path' then this file system object is atomically deleted and replaced with the source file or directory.

#### SHELL [-c | --command] [string]

Starts a new shell instance as a child process of the current shell. The new shell inherits all environment variables, the root directory and the current working directory of its parent shell. Exit the new shell by issuing an "exit" command on the command line. The shell runs in interactive mode by default.

If a string is provided as the last argument to the shell then this string is interpreted as the path to a shell script. The shell loads the script and executes it. The '--command' switch causes the shell to interpret the string as a shell command instead of a path to a script file. Multiple commands may be provided in a single string by separating them with a semicolon (';') character.



#### TYPE [--hex] \<path>

Prints the contents of the file 'path' to the console. Pass the switch '--hex' to print the file contents as a series of hexadecimal numbers.
