# The Serena Shell Language

This document gives a formal definition of the Serena shell language grammar rules.

_Note:_ The current version of the shell only implements a small subset of the language described below.

## Semantic Rules

These are the semantic rules of the language:

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
  * The result of an if-then expression is the result of the then block if the condition evaluates to true and Undefined if it evaluates to false
  * The result of a while expression is the result of the last expression in the loop body if the loop body is executes at least once
  * The result of a while expression that never executes its loop body is Undefined
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
  * Void: the unit type
  * Undefined: the uninhabited bottom type
  * A Void value can not be assigned to a variable
  * A Undefined value can be assigned to a variable, whoever any attempt to read/refer to the value will trigger a script termination

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
  
## Tokens

```
//
// Default Mode
//

NL
    : \x0a | \x0d \0xa
    ;

ELSE:       'else';
FALSE:      'false';
IF:         'if';
INTERNAL:   'internal';
LET:        'let';
PUBLIC:     'public';
TRUE:       'true';
VAR:        'var';
WHILE:      'while';

AMPERSAND:          '&';
ASSIGNMENT:         '=';
ASTERISK:           '*';
BANG:               '!';
BAR:                '|';
CLOSE_BRACE:        '}';
CLOSE_PARA:         ')';
CONJUNCTION:        '&&';
DISJUNCTION:        '||';
EQEQ:               '==';
GREATER:            '>';
GREQ:               '>=';
LEEQ:               '<=';
LESS:               '<';
MINUS:              '-';
NOEQ:               '!=';
OPEN_BRACE:         '{';
OPEN_PARA:          '(';
PLUS:               '+';
SEMICOLON:          ';';
SLASH:              '/';

DOUBLE_BACKTICK(default, dbt_mode): '``';
DOUBLE_QUOTE(default, dq_mode):     '"';

STRING_SEGMENT(dq_mode)
    : [^\$"]
    ;

STRING_SEGMENT(dbt_mode)
    : [^\$``]
    ;

ESCAPE_SEQUENCE(dq_mode, dbt_mode)
    : '\' [abefvrn$"'\]
    ;

ESCAPED_EXPRESSION(dq_mode, dbt_mode)
    : '\('
    ;

SINGLE_QUOTED_STRING
    : '''[^']'''
    ;

SINGLE_BACKTICK_STRING
    : '`'[^`]'`'
    ;

INTEGER
    : ('0'[box])? [0-9]+
    ;

VAR_NAME(default, dq_mode, dbt_mode)
    : '$' (('_' | [a-z] | [A-Z] | [0-9])* ':')? ('_' | [a-z] | [A-Z] | [0-9])+
    ;

IDENTIFIER
    : ([^\0x20\0x09\0x0b\0x0c|&+-*/\;$"`'(){}<>=!] | ('\'?))+
    ;
```

## Grammar Rules

```
script
    : expressions EOF
    ;

expressions:
    : expression*
    ;

expression
    : (varDeclExpression
        | assignmentExpression
        | arithmeticExpression
        | breakExpression
        | CONTINUE)? terminator
    ;

terminator
    : NL | SEMICOLON | AMPERSAND
    ;

varDeclExpression
    : (INTERNAL | PUBLIC)? (LET | VAR) VAR_NAME ASSIGNMENT arithmeticExpression
    ;

assignmentExpression
    : assignableExpression ASSIGNMENT arithmeticExpression
    ;

assignableExpression
    : VAR_NAME
    ;

breakExpression
    : BREAK arithmeticExpression?
    ;

command
    : commandPrimaryFragment commandSecondaryFragment*
    ;

commandPrimaryFragment
    : SLASH
    | IDENTIFIER
    | SINGLE_BACKTICK_STRING
    | doubleBacktickString
    ;

commandSecondaryFragment
    : IDENTIFIER
    | ELSE
    | FALSE
    | IF
    | INTERNAL
    | LET
    | VAR
    | WHILE
    | PUBLIC
    | TRUE
    | ASSIGNMENT
    | CONJUNCTION
    | DISJUNCTION
    | PLUS
    | MINUS
    | ASTERISK
    | SLASH
    | EQEQ
    | NOEQ
    | LEEQ
    | GREQ
    | LESS
    | GREATER
    | VAR_NAME
    | SINGLE_BACKTICK_STRING
    | doubleBacktickString
    | literal
    | parenthesizedArithmeticExpression
    ;

pipeline
    : (disjunction | command) BAR command (BAR command)*
    ;

arithmeticExpression
    : disjunction
    | command
    | pipeline
    ;

disjunction
    : conjunction (DISJUNCTION conjunction)*
    ;

conjunction
    : comparison (CONJUNCTION comparison)*
    ;

comparison
    : addition (comparisonOperator addition)*
    ;

comparisonOperator
    : EQEQ
    | NOEQ
    | LEEQ
    | GREQ
    | LESS
    | GREATER
    ;

addition
    : multiplication (additionOperator multiplication)*
    ;

additionOperator
    : PLUS
    | MINUS
    ;

multiplication
    : prefixUnaryArithmetic (multiplicationOperator prefixUnaryArithmetic)*
    ;

multiplicationOperator
    : ASTERISK
    | SLASH
    ;

prefixOperator:
    : PLUS
    | MINUS
    | BANG
    ;

prefixUnaryArithmetic
    : prefixOperator* primaryExpression
    ;

primaryExpression
    : literal
    | VAR_NAME
    | parenthesizedArithmeticExpression
    | conditionalExpression
    | loopExpression
    ;

parenthesizedArithmeticExpression
    : OPEN_PARA arithmeticExpression CLOSE_PARA
    ;

conditionExpression
    : ifExpression
    ;

ifExpression
    : IF arithmeticExpression block (ELSE block)?
    ;

loopExpression
    : whileExpression
    ;

whileExpression
    : WHILE arithmeticExpression block
    ;

block
    : OPEN_BRACE statements CLOSE_BRACE
    ;

literal
    : FALSE
    | TRUE
    | INTEGER
    | SINGLE_QUOTED_STRING
    | doubleQuotedString
    ;

doubleBacktickString
    : DOUBLE_BACKTICK
        ( STRING_SEGMENT(dbt_mode)
        | ESCAPE_SEQUENCE(dbt_mode)
        | escapedArithmeticExpression(dbt_mode)
        | VAR_NAME(dbt_mode)
      )* DOUBLE_BACKTICK(dbt_mode)
    ;

doubleQuotedString
    : DOUBLE_QUOTE
        ( STRING_SEGMENT(dq_mode)
        | ESCAPE_SEQUENCE(dq_mode)
        | escapedArithmeticExpression(dq_mode)
        | VAR_NAME(dq_mode)
      )* DOUBLE_QUOTE(dq_mode)
    ;

escapedArithmeticExpression
    : ESCAPED_EXPRESSION arithmeticExpression(default) CLOSE_PARA(default)
    ;
```

### Informal Grammar Rules

* Prefix operators must exhibit whitespace on the left side and no whitespace on the right side
* Postfix operators must exhibit whitespace on the right side and no whitespace on the left side
* Infix operators must either exhibit whitespace on both sides or on neither side
* The last expression in a script file is accepted even if there is no '\n', ';' or '&' terminator
* The last expression inside a block is accepted even if there is no '\n', ';' or '&' terminator
* The 'break' and 'continue' keywords may only appear inside of a loop body
* A character sequence enclosed by single or double backticks indicates that the character sequence names an external command
* A command name is the longest sequence of a commandPrimaryFragment followed by 0 or more command SecondaryFragment instances which are not separated by whitespace
* A single command parameter is the longest sequence of commandSecondaryFragment instances which are not separated by whitespace
* a string that is enclosed in single or double backticks and that appears as part of a command name forces the resolution of the command name to an external command. Thus none of the internal commands are considered in this case
* a string that is enclosed in single or double backticks and that appears as part of a command parameter is treated as a plain string with no special semantics
* Escaping a character outside of a double quoted string effectively turns the escaped character into an identifier character
* Escaping the LF or CRLF character(s) outside a double quoted string replaces the LF/CRLF sequence with an empty string and does not terminate the identifier that is currently being lexed
* Adjacent tokens are merged into a single token under the following circumstances:
  * A sequence of non-whitespace separated IDENTIFIER tokens
  * A homogenous or heterogenous sequence of non-whitespace separated single and double quoted strings
  * A homogenous or heterogenous sequence of non-whitespace separated single or double backtick strings
* The result of evaluating a variable reference or a sub-expression is merged without intervening whitespace with the result of surrounding rules/tokens, if:
  * A homogenous or heterogenous sequence of non-whitespace separated variable references and sub-expressions
  * heterogenous sequence of variable references, sub-expressions and IDENTIFIER tokens
