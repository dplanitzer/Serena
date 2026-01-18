# Quick Tour

The shell supports a wide array of functionality. From running executables, to printing text to the screen, doing mathematical calculations, to to interactive text input, etc.

The following line is an example of how to print text to the screen. The 'cls' command clears the screen and the 'echo' command prints the text in double quotes to the screen:

```
cls
echo "Hello World!"
```

We can even do math right inside the text that we want to print to the screen by writing the math instructions inside an 'expression escape sequence' which is written as \\(X). The X between the parenthesis represents the mathematical expression that the shell should evaluate before it prints the result:

```
echo "The sum of '1 + 2' is \(1 + 2)"
```

Sometimes we want to be able to decide at runtime what code to execute based on the result of some computation. We can do this using an if-then-else expression:

```
let $r = if 1 < 2 { "That"s correct" } else { "That's not quite right" }
```

This example shows that we are able to assign the result of the then or the else block of an if-then-else, to a new variable. The 'let' instruction tells the shell that we want to create a new read-only variable. This is a variable that once assigned, can not be changed anymore. The 'if' instruction causes the shell to evaluate the conditional expression (which is 1 < 2 in this example). If the conditional expression evaluates to true then the then block is executed next; if however the conditional expression evaluates to false then the else block is evaluated next.
The code area between the opening and closing braces is known as a 'code block'. All instructions inside a code block are executed sequentially from the first to the last instruction. Note that the result of the code block is the result of the last instruction in the code block.

Now sometimes we want to be able to execute a list of instructions more than once. We can do this with a while loop:

```
var $i = 0

while $i < 10 {
    echo $i
    $i = $i + 1
}

echo "Done!"
```

Here we first create a new mutable variable called 'i'. Note that variable names always have to be prefixed with a '$' character. This is true whether you want to read or write the variable.
The loop is implemented by the while instruction. It evaluates the conditional expression that follows and it then executes the loop body block as long as the conditional expression continues to evaluate to true. Once the conditional expression evaluates to false the loop ends and execution continues with the first instruction that follows the end of the loop body.

Finally the Serena shell allows you to invoke a large number of internal commands and external programs. Better yet, you can easily combine the power of expressions with commands:

```
let $i = 10

makedir "test_\($i + 1)" $i (1000 / 2)
```
The 'makedir' command is used to create one or more directories. It expects a list of directory names and it creates new directories with those names.
In this example, the first directory is named "test_11", the second is named "10" and the third is named "500".
