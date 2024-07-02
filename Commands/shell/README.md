# The Shell

The shell enables you to launch executables, navigate the file system, list the contents of directories, work with files and other things. A number of commands are built into the shell. You can find a description of each command in the list below.

## Line Editor

You enter shell commands in the shell's line editor. The line editor allows you to enter, edit and recall previously entered commands. Note that the line editor is currently limited to override/replace mode.

To enter a command, type it in the currently active line and hit the return/enter key. This triggers the execution of the command and it adds the command to the command history. A previously entered command can be recalled by pressing the cursor-up key. You can scroll through all commands in the command history by pressing the cursor-up and cursor-down keys.

Some simple editing functions are supported. Backspace moves the cursor one column to the left, the cursor-left and right keys move the cursor left and right. Ctrl-a moves the cursor to the beginning of the line and cursor-e moves the cursor to the end of the line. Ctrl-l clears the screen.

## The Shell Language

The shell language is primarily inspired by the AmigaDOS, Fish shell and the Windows PowerShell, plus some
(light) influence from zsh, bash and finally a healthy dose of new
design elements.

### Language Basics

* A shell script is a sequence of sentences
* A sentence is a sequence of words, terminated by a sentence terminator
* The first word in a sentence is interpreted as an internal or external command and all other words in the sentence are treated as arguments to the command
* All words in a sentence are separated by whitespace
* A word is a sequence of morphemes which are the constituent parts of a word and which are directly adjacent to each other (there is no whitespace between morphemes)
* Note however that a morpheme may in fact contain whitespace in some circumstances. Ie if the morpheme is a quoted string then there may be whitespace inside the string
* Morphemes are:
  * An unquoted sequence of non-whitespace characters
  * Single quoted strings
  * Double quoted strings
  * Backslash followed by a character (quoted character)
  * $ followed by an identifier (variable reference)
  * Nested sentences (block) enclosed in ( and )
* Sentence terminators are:
  * The end of the input (EOF)
  * Newline
  * The ; character (command will be executed synchronously)
  * The & character (command will be executed asynchronously)
  * The ) character if the command appears inside a block

### Quoting Characters

You can quote a character on the command line by prefixing it with a '\\' (backslash) character. Quoting a character prevents the shell from interpreting the character and it is instead treated as the literal character that has no special meaning attached to it.

For example, the sequence '\n' causes the shell to insert the literal character 'n' into the command line and the sequence '\$' prevents the shell from interpreting the \$ as the beginning of a variable reference. Instead the \$ is inserted into the command line as the literal \$ character.

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

### Blocks (Nested Sentences)

A sentence may be nested inside another sentence by enclosing the nested sentence in parentheses like this:

```
echo (seq 5)
```

The shell executes the nested sentence first and it then replaces the nested sentence with the output of all the commands of the nested sentence. This is similar to the \$( ... ) substitution command in POSIX shells except that the leading $ is not needed.

### Commands

#### CD \<path>

Set the current working directory to the directory indicated by the provided path.

#### CLS

Clears the screen.

#### DELETE \<path ...>

Deletes one or more directories or files indicated by the provided path(s). A directory must be empty to be eligible for deleting.

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

#### SHELL [path ...]

Starts a new shell instance as a child process of the current shell. The new shell inherits all environment variables, the root directory and the current working directory of its parent shell. You can exit the new shell with the "exit" command. The shell runs in interactive mode by default. You can pass one or more paths to shell scripts when you invoke a shell and the shell will then run in batch mode and execute one shell script after the other. Shell scripts are executed in the order in which they are passed to the shell executable.

#### TYPE [--hex] \<path>

Prints the contents of the file 'path' to the console. Pass the switch '--hex' to print the file contents as a series of hexadecimal numbers.
