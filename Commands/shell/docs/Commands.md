# Commands

## `break`

Ends a loop and optionally causes the loop to return the result of the provided expression. A loop is an expression and thus a loop returns a result. The break command may be used to end a loop prematurely and to cause it to return the value of the expression that is passed to the break command.

The break command evaluates to void if no expression is provided.

Note that a break command may only appear inside of a loop body.

### Signature

`break [expr]`

### Parameters

* `expr`: the expression that should be evaluated to calculate the result value of a loop

## `cd`

Set the current working directory to the directory indicated by the provided path.

### Signature

`cd <path>`

### Parameters

* `path`: path to a directory

## `cls`

Clears the screen.

### Signature

`cls`

## `continue`

Causes a return to the beginning of the loop.

Note that the continue command may only appear inside of a loop body.

### Signature

`continue`

## `copy`

Copies a file from its current location in the filesystem to a new location in the filesystem hierarchy. Note that copying directories is currently not supported. The copy operation will preserve the permissions of the source file. However the user and group are assigned based on the user and group of the logged in user. The destination path may be a full path which specifies the destination directory and the filename. Or it may just specify the directory. The name and extension of the source file are preserved in this case.

### Signature

`copy <src_path> <dst_path>`

### Parameters

* `src_path`: relative or absolute path to the file to copy
* `dst_path`: relative or absolute path to the file to the destination directory where teh file should be copied to. May optionally include the filename of the new file

## `delete`

Deletes one or more directories or files indicated by the provided path(s). A directory must be empty to be eligible for deleting.

### Signature

`delete <path ...>`

### Switches

* `--verbose, -v`: causes delete to print the path of objects as they are being deleted

### Parameters

* `path ...`: one or more paths to files or directories

## `disk format`

Initializes an unformatted disk with the specified filesystem. The filesystem is set up with an empty root directory. Note that the only filesystem supported at this time is SerenaFS.

### Signature

`disk format <disk_path>`

### Switches

* `--type, -t`: specifies the name of the filesystem to create
* `--label, -l`: specifies the label that should be assigned to the disk
* `--owner, -o`: specifies the user and group id for the root directory
* `--permissions, -p`: specifies the permissions of the root directory
* `--quick, -q`: tells the tool that it should skip cleaning all disk blocks by setting their contents to 0

### Parameters

* `disk_path`: a path to a disk driver (typically found in `/dev/hw`)

### Example

`disk format -t sefs /dev/hw/fd-bus/fd1`

The disk in floppy disk drive 1 is formatted and initialized with a fresh Serena Filesystem copy. Note that it will take a while to complete the formatting process. Once completed the disk may be mounted and then accessed through the mountpoint directory.

## `disk fsid`

Prints the globally unique filesystem id of the filesystem on which the current working directory resides if no path is provided, or the filesystem id of the filesystem on which the file/directory resides to which the provided path points.

### Signature

`disk fsid [path]`

### Parameters

* `path`: a path to a file or directory

## `disk geometry`

Prints information about the geometry of the disk on which a filesystem is stored. The command prints information for the disk that holds the current working directory by default. Specify a path to a disk drive instead to print the geometry of this disk drive and specify a path to filesystem instead to print geometry information about the disk holding this filesystem.

### Signature

`disk geometry [path]`

### Parameters

* `path`: a path to a file, directory or disk device

## `disk info`

Prints information about a filesystem. Eg the size, number of used and freed blocks, name of the underlying disk device, etc. The command prints information for the filesystem that holds the current working directory by default. Specify the id of a mounted filesystem to print information about this particular filesystem instead.

### Signature

`disk info [path]`

### Parameters

* `path`: a path to a file or directory

## `disk mount`

Mounts the filesystem stored on a disk and makes it accessible at the directory designated by the `--at` switch. Applications are able to access the filesystem once the mount operation has successfully completed.

### Signature

`disk mount <disk_path>`

### Switches

* `--at, -t`: specifies the directory at which the filesystem should be mounted. This switch is required

### Parameters

* `disk_path`: a path to a disk driver (typically found in `/dev/hw`)

### Example

`disk mount --at /Volumes/FD1 /dev/hw/fd-bus/fd1`

Mounts the disk in floppy disk drive 1 at the directory `/Volumes/FD1`. Once mounting has completed, the contents of the disk may be accessed through this directory path.

## `disk unmount`

Unmounts a filesystem. This command ensures that all cached data is first written to the disk and the the filesystem is unmounted.

### Signature

`disk unmount <at_path>`

### Switches

* `--force, -f`: forces the unmount to proceed even if some application is still holding a file on the disk open

### Parameters

* `at_path`: the path to the directory at which the filesystem is mounted

### Example

`disk unmount /Volumes/FD1`

Unmounts the filesystem mounted at the directory `/Volumes/FD1`. All data that is stored in the disk cache and hasn't been written to the disk yet is automatically written to the disk before the filesystem is unmounted. Once unmounted the contents of the filesystem is no longer accessible until it is mounted again.

## `echo`

Prints the given list of strings separated by single space characters to standard out. The list of strings is followed by a single newline character. Note that all strings following a '--' parameter are printed verbatim and not interpreted as an option even if they start with '-' or '--' characters.

### Signature

`echo [-n | --noline] [-s | --nospace] <strings ...>`

### Switches

* `--noline, -n`: tells echo that it should not print an extra newline at the end of the line
* `--nospace, -s`: tells echo that it should not print an extra space between adjacent strings

### Parameters

* `strings ...`: one or more strings to print

## `exists`

Checks whether the file or directory at path 'path' exists and returns true if that is the case and false otherwise.

### Signature

`exists <path>`

### Parameters

* `path`: path to a directory or file

## `exit`

Exits the current shell with the exit code 'exit_code'. The exit code is passed to the program that invoked the shell. An exit code of 0 is assumed if no exit code is provided.

### Signature

`exit <exit_code>`

### Parameters

* `exit_code`: an integer that will be passed to the parent shell

## `history`

Prints the contents of the shell history to standard out. Each history entry is printed as a separate line. The entries are printed in the order of newest to oldest.

### Signature

`history`

## `id`

Prints the real user and group id of the logged in user.

### Signature

`id`

## `if`

Causes the shell to evaluate the provided conditional expression to decide whether execution should continue with the provided then or else block. The else block is optional and execution continues with the first expression following the end of the then block if the conditional expression evaluates to false and there is no else block.

Note that an if command is an expression. This means that the result of the if expression is the result of the then block if the conditional expression evaluates to true, and the result of the else block otherwise.

### Signature

`if <cond_expr> <then_block> [else <else_block>]`

### Parameters

* `cond_expr`: the conditional expression
* `then_block`: the block to execute when the conditional expression evaluates to true
* `else_block`: the block to execute when the conditional expression evaluates to false

## `input`

Prints the prompt 'prompt' if provided and then allows the user to enter a line of text. Then returns the text that the user has entered.

### Signature

`input [prompt]`

### Parameters

* `prompt`: a string

## `let`

Defines a new immutable named variable in the current scope. An error is generated if a variable with that name already exists in the current scope. However, it is permissible to shadow a variable with the same name that was defined in one of the parent scopes.

The new variable is assigned the result of evaluating the provided initialization expression. Note that once created, the value of the variable can not be changed anymore.

The variable ceases to exist at the end of the current scope.

### Signature

`let <var_name> = <expr>`

### Parameters

* `var_name`: a variable name
* `expr`: an expression that is evaluated to calculate the value that should be assigned to the variable

## `load`

Loads and returns the contents of a text file.

### Signature

`load <path>`

### Parameters

* `path`: path to a text file

## `list`

Lists the contents of one or more directories as indicated by the provided paths. Lists the contents of the current working directory if no path is provided. By default the list command does not list entries that start with a '.'. Pass the '--all' parameter to enable the output of all directory entries.

### Signature

`list [-a | --all] [path ...]`

### Switches

* `--all, -a`: tells the list command to show all files and directories, even hidden ones

### Parameters

* `path ...`: one or more paths to directories

## `makedir`

Creates one or more empty directories at the file system locations designated by the provided paths. The last path component in a path specifies the name of the directory to create. Any non-existing intermediate directories are automatically created if you pass the '--parents' option.

### Signature

`makedir [-p | --parents] <path ...>`

### Switches

* `--parents, -p`: tells the makedir command that it should automatically create missing intermediate directories

### Parameters

* `path ...`: one or more paths to directories that should be created

## `popcd`

Pops the least recently saved current directory from the current directory stack and makes it the new current directory. Prints an error if the stack is empty.

### Signature

`popcd`

## `pushcd`

Saves the current directory on a stack and then makes `path` the new current directory if a path is specified.

### Signature

`pushcd [path]`

### Parameters

* `path`: path of the directory to make current

## `pwd`

Returns the absolute path of the current working directory.

### Signature

`pwd`

## `rename`

Renames the file or directory located at a source path, to a file or directory at a destination path. If a file or directory already exists at the destination path then this file system object is atomically deleted and replaced with the source file or directory.

### Signature

`rename <src_path> <dst_path>`

### Parameters

* `src_path`: path to a source file or directory
* `dst_path`: path to a destination directory or file

## `shell`

Starts a new shell instance as a child process of the current shell. The new shell inherits all environment variables, the root directory and the current working directory of its parent shell. Exit the new shell by issuing an "exit" command on the command line. The shell runs in interactive mode by default.

If a string is provided as the last argument to the shell then this string is interpreted as the path to a shell script. The shell loads the script and executes it. The '--command' switch causes the shell to interpret the string as a shell command instead of a path to a script file. Multiple commands may be provided in a single string by separating them with a semicolon (';') character.

### Signature

`shell [-c | --command] [-l | --login] [string]`

### Switches

* `--command, -c`: tells the shell that it should interpret the provided string as a command and execute it
* `--login, -l`: tells the shell that it is the login shell

### Parameters

* `string`: either the name of a script file or a string that should be interpreted as a command to execute

## `save`

Writes the provided data to a new file at location 'path'. Any existing data is by default overridden. The provided data is appended to the end of an existing file if the '--append' switch is provided.

### Signature

`save [-a | --append] [-r | --raw] <data> to <path>`

### Switches

* `--append, -a`: tells the save command that it should append new data to an already existing file
* `--raw, -r`: tells the save command that the input data is raw bytes rather than text

### Parameters

* `data`: the input data which may be text or raw bytes
* `path`: path to a file

## `shutdown`

Prepares the computer for power off by writing still cached data to disk. It is safe to turn off power once this command has finished running.

### Signature

`shutdown`

## `status`

Prints information about all active processes in the system.

### Signature

`status`

## `type`

Prints the contents of the file 'path' to the console. Pass the switch '--hex' to print the file contents as a series of hexadecimal numbers.

### Signature

`type [--hex] <path>`

### Switches

* `--hex`: tells the type command that it should display the file contents in the form of a hex-dump

### Parameters

* `path`: path to a text or binary file

## `uptime`

Returns the number of milliseconds that have elapsed since boot.

## `var`

Defines a new mutable named variable in the current scope. An error is generated if a variable with that name already exists in the current scope. However, it is permissible to shadow a variable with the same name that was defined in one of the parent scopes.

The new variable is assigned the result of evaluating the provided initialization expression. Note that the type of the variable may be changed later to a different type by assigning a value with a different type to the variable.

The variable ceases to exist at the end of the current scope.

### Signature

`var <var_name> = <expr>`

### Parameters

* `var_name`: a variable name
* `expr`: an expression that is evaluated to calculate the value that should be assigned to the variable

## `vars`

Lists all variables defined in the current scope. Local (internal) variables are listed first and public (environment) variables are listed second.

### Signature

`vars`

## `wait`

Pauses execution for the specified amount of milliseconds, seconds, minutes or hours.

### Signature

`wait <duration>[ms, s, m, h]`

### Parameters

* `duration`: a positive integer giving a duration in milliseconds, seconds, minutes or hours

## `while`

Defines a while loop that executes its loop body as long as the conditional expression continues to evaluate to true. Once the conditional expression evaluates to false, the loop ends and execution continues with the first expression that follows the loop body.

A while loop is an expression. The result of the while expression is the result of the last expression inside the loop body if the loop is not prematurely ended by executing a break expression. If a loop is ended prematurely with the help of a break expression, then the value of the break expression is the result of the while loop.

### Signature

`while <cond_expr> <body>`

### Parameters

* `cond_expr`: a conditional expression
* `body`: the loop body
