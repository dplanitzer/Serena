# Variables

The Serena shell supports immutable and mutable variables. An immutable variable is a variable that can not be changed after it has been declared. A mutable variable on the other hand allows changing the value (and type) of the variable as often as you like.

A variable has to be declared with the `let` or `var` keyword before it can be used or reassigned. Use the `let` keyword to create an immutable variable and the `var` keyword to create a mutable variable. The following example shows how to declare variables:

```
let $a = 10         # Declares an immutable variable
var $b = "Hello"    # Declares a mutable variable
```

Note that a variable must be initialized with the value of an expression when you declare it.

Variable names must be prefixed with a dollar sign. This is true no matter whether you just want to get the value of a variable, declare it or change it.

The value of a mutable variable may be changed at any time simply by assigning the value of an expression to it. The new value may even be of a different type:

```
var $b = "Hello"

...

$b = 10
```

Although we assign an integer value to variable 'b', which up to this point held a string, the assignment is carried out and the type of the variable is changed to integer.

A variable has an access mode:

```
internal var $x = 10

public var $HOME = "/Users/johndoe
```

The access mode determines whether an external command or process can access the variable or not. An internal variable is only accessible to the shell and shell scripts started from the shell instance in which the variable was declared. A public variable is accessible to external commands and processes that are spawned from the shell in which the public variable was declared. Public variables is what is commonly known as environment variables in other shells.

## Types

Every variable has a type. Initially the type of the variable is determined by the type of the value that is assigned to the variable at declaration time. Later on the type is changed to reflect the most recently assigned value.

The following types are supported:

| Type  | Description |
|-------|-------------|
| Bool | Boolean values |
| Integer | 32bit integer values |
| String  | Character strings |

## Literals

A variable may be initialized with a literal value. The literal value for an integer is just a sequence of decimal digits. The literal for a string is a single quoted or double quoted piece of text and the literal for a boolean value are the keywords `true` and `false`:

```
# An integer literal
$b = 1234

# A single quoted string literal. Every character is considered part of the string value.
$b = 'This will cost a lot $$$'

# A double quoted string literal. This kind of literal supports escape sequences, embedded variable names and expressions
$b = "Hello $NAME, is 1+2+3 really \(1+2+3)?"

# A boolean literal
$b = true
$b = false
```

## Lifetime and scopes

The lifetime of a variable is limited to the scope in which it is declared. A variable begins life at the point of its declaration and it ceases to exist at the end of the scope in which it is declared.

The name of a variable must be unique in the scope in which it is declared. So once a variable with name 'x' has been declared in a scope 's', no other variable can be declared in scope 's' with the same name of 'x'.

However, it is permissible to declare a variable in a child scope with a name 'x' even if a variable with the same name already exists in one of the parent scopes. The newly declared variable 'shadows' the like-named variable from the parent scope for the duration of the child scope.

Variable names support a special notation which allows you to refer to a variable in a specific scope:

```
echo $global:x      # Prints the value of the global variable 'x'

echo $script:x      # Prints the value of the script variable 'x'
```

A global variable is a variable that was declared on the shell level. Typically environment variables are declared on this level. A script variable is a variable that was declared in a shell script.

## Environment Variables

Environment variables are also known as public variables in the Serena shell. Once declared they are accessible to all processes that are direct or indirect children of the shell instance in which the variable was declared.

Here is an example of how you can declare an environment variable with name 'A':

```
public var $X = "Hello"
```

Note that environment variables use all uppercase letters by convention.

Environment variables are scoped just like internal variables are. This means that an environment variable ceases to exist in the shell process at the point where the scope in which it is declared ends. Note however that since each child process receives a copy of all environment variables when it starts up, that the copy of the variable continues to exist inside a child process as long as the child process does not exit.

## Listing Variables

You can list all the variables accessible in the current scope with the help of the `vars` command. It shows the name and values of internal and public variables defined in the current scope and all parent scopes.
