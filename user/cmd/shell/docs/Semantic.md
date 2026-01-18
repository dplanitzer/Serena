# Semantic Rules

Here are the semantic rules of the language:

* Expressions
  * Everything is an expression
  * An expression either succeeds and produces a value or it fails
  * An expression that fails leads to the termination of the script
  * The shell guarantees that no more side-effects will be produced after detecting an error and that the script will be immediately terminated
  * The value of a block/expression list is the value of the last expression in the expression list
  * A variable declaration produces a value of type Void
  * An assignment expression produces a value of type Void
  * A command whose stdout is captured produces a value of type String; a command whose stdout is not captured produces a value of type Void
  * A pipeline whose stdout is captured produces a value of type String; a pipeline whose stdout is not captured produces a value of type Void

* Flow Control Expressions
  * All flow control constructs may be used in an expression or statement context
  * The result of a flow control expression that is used in a statement context is ignored
  * The result of an if-then-else expression is the result of the then block if the condition evaluates to true and the result of the else block if the condition evaluates to false
  * The result of an if-then expression is the result of the then block if the condition evaluates to true and Void if it evaluates to false
  * The result of a while expression is the result of the last expression in the loop body if the loop body is executed at least once
  * The result of a while expression that never executes its loop body is Void
  * The result of a break expression is the result of the associated arithmetic expression
  * The result of a break expression that does not come with an associated arithmetic expression, is Void
  * The result of a continue expression is Void

* Commands:
  * A command may take any number of parameters. How those parameters are interpreted is up to the command
  * A command is expected to return a status. 0 means that the command was successful; A value != 0 indicates that the command has failed
  * A command produces a result/value by writing data to stdout
  * The result/value of a command is by default not captured by the language runtime. A command must either appear in a context that implicitly triggers value capture (i.e. assignment) or it must be enclosed by '(' and ')' to capture the result.
  * The captured result/value of a command becomes the value of the command invocation expression
  * The result/value of a command that appears on the left side of the pipe operator '|' is always captured
  * A command must be enclosed in parenthesis if it should be used as a factor inside an expression. The reason for this requirement is that this way it is obvious how far the (parameter list) of the command extends. I.e. it is clear that 'foo + 1' is always the command 'foo' followed by the command parameters '+' and '1'

* Pipelines
  * A pipeline is a concatenation of commands where the output of the command to the left of the pipeline symbol '|' is captured and made available as the input to the command on the right side of the pipeline symbol
  * The first command of a pipeline may be an arithmetic expression. The value of the arithmetic expression is calculated and fed as input to the next command in the pipeline

* Values
  * Every value has a type
  * A value of type Void can not be assigned to a variable
  * A value of type Void does not support any operations and thus can not be used as an argument in an arithmetic expression
  * A command and pipeline which does not capture a value (aka the stdout channel of the command/pipeline is not being recorded) produces a value of type Void

* Value Types
  * String: a sequence of characters
  * Int: a 32bit signed integer value
  * Void: the unit type which has exactly one value: void
  * Never: the uninhabited bottom type (has no value at all)
  * Note that the Void value can be assigned to a variable
  * There is no way to produce a value for Never. The script is terminated if a value would have to be produced for it

* Scopes:
  * The bottom most scope is the global scope which is established when the shell starts up
  * Every shell script invocation is associated with a scope
  * Every block inside a shell script is associated with a scope
  * Variables which are defined inside a scope seize to exist at the end of the block
  * A variable definition in a child scope S shadows a like-named variable defined in one of the parent scopes
  * Scopes are organized in a stack
  * A variable name may be prefixed with one of the following scope selectors to address a variable in that scope:
    * 'global': the global scope
    * 'script': the script scope
    * 'local': the local scope. This is the scope that is used if no scope selector is specified

* Variables:
  * Are scoped. A variable is bound to a scope at definition time and it remains in this scope until the variable is destroyed because the scope ends.
  * Are dynamically typed. The type of the variable is the type of the most recently assigned value
  * Have an access mode:
    * 'internal': the variable is only accessible to the currently executing script. This is the default access mode
    * 'public': the variable is accessible to the executing script and any of the processes the script runs. Public variables are made available to processes in the form of environment variables
