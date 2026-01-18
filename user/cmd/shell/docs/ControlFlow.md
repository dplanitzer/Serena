# Control Flow

The Serena shell supports a number of control flow constructs which you can use for example, to control how a shell script should react to user input.

Control flow in the Serena shell language are expressions just like everything else. Nevertheless a control flow construct may either be used expression-like or statement-like. The first form means that the result of the flow control construct is assigned to a variable while the second form means that the potential result of the flow control expression is ignored and not assigned to a variable.

## Conditionals

First, a conditional evaluates an expression and it then continues execution with the then or the else block depending on whether the conditional expression evaluated to true or false:

```
# This is a statement-like conditional
if $i < 10 {
    echo "Less than 10"
}
else {
    echo "Greater or equal 10"
}

# This is an expression-like conditional which calculates min(x, y)
let $r = if $x < $y { $x } else { $y }
```

Both the then and the else blocks must be enclosed in braces. The last expression in each block determines the result value of the block and it is this result value that is returned by the if-then conditional as its result value.

## While Loops

A while loop evaluates an expression at each loop turn to decide whether it should continue with the loop or whether it should end the loop. The while loop repeatedly executes the loop body as long as the loop conditional expression continues to evaluate to true.

Like conditionals, a while loop may be used in statement or expression form. In expression form the result of the while loop is either defined by the last expression in the loop body or the expression that is passed to the `break` keyword, if the loop is terminated by the break keyword.

```
# A statement-like while loop
while $i < 10 {
    echo $i
}

# An expression-like while loop. The result of the while loop is the value of $i
let $r = while $i < 10 {
    echo $i
    $i
}
```

The body of a while loop may contain the special keywords `continue` and `break`. Use `continue` to prematurely finish a loop iteration and to send execution back to the beginning of the loop:

```
while $i < 10 {
    if ($i % 2) == 0 {
        continue
    }

    echo $i
}
```

In this example the while loop executes the `echo` only if the variable `$i` holds an odd integer since execution is sent back to the beginning of the loop every time the variable holds an even integer.

The `break` keyword may be used to prematurely terminate a while loop altogether. The break keyword optionally takes an expression which it evaluates to calculate the value that should be returned by the while loop:

```
$var $i = 0

let $r = while true {
    let $x = input "Enter some text: "

    if $x == "end" {
        break $i
    }

    echo $x
    $i = $i + 1
}
```

This example repeatedly requests the user to enter some text and it counts how often the user enters text until he enters 'end'. The loop is terminated once the user enters 'end' and the break keyword returns the value of `$i` as the value of the while loop.
