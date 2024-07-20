# The Serena Shell Language

This document gives a formal definition of the Serena shell language grammar rules.

_Note:_ The current version of the shell only implements a small subset of the language described below.

## Semantic Rules

These are the semantic rules of the language:

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
  * Are typed
  * Have an access mode:
    * 'internal': the variable is only accessible to the currently executing script. This is the default access mode
    * 'public': the variable is accessible to the executing script and any of the processes the script runs. Public variables are made available to processes in the form of environment variables
* Commands:
  * A command may take any number of parameters. How those parameters are interpreted is up to the command
  * A command is expected to return a status. 0 means that the command was successful; A value != 0 indicates that the command has failed
  * A command produces a result/value by writing data to stdout
  * The result/value of a command is by default not captured by the language runtime. A command must be enclosed by '(' and ')' to capture the result.
  * The captured result/value of a command becomes the value of the command invocation expression
  * The result/value of a command that appears on the left side of the pipe operator '|' is always captured
  * A command must be enclosed in parenthesis if it should be used as a factor inside an expression. The reason for this requirement is that this way it is obvious how far the (parameter list) of the command extends. I.e. it is clear that 'foo + 1' is always the command 'foo' followed by the command parameters '+' and '1'
* Pipelines
  * A pipeline is a concatenation of commands where the output of the command to the left of the pipeline symbol '|' is captured and made available as the input to the command on the right side of the pipeline symbol
  * The first command of a pipeline may be an expression. The value of the expression is calculated and fed as input to the next command in the pipeline
  
## Tokens

```
//
// Default Mode
//

NL
    : \x0a | \x0d \0xa
    ;

ELSE:       'else';
IF:         'if';
INTERNAL:   'internal';
LET:        'let';
PUBLIC:     'public';
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
    : statements EOF
    ;

statements:
    : statement*
    ;

statement
    : (varDeclaration
        | assignmentStatement
        | expression)? statementTerminator
    ;

statementTerminator
    : NL | SEMICOLON | AMPERSAND
    ;

varDeclaration
    : (INTERNAL | PUBLIC)? (LET | VAR) VAR_NAME ASSIGNMENT expression
    ;

assignmentStatement
    : expression ASSIGNMENT expression
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
    | IF
    | INTERNAL
    | LET
    | VAR
    | WHILE
    | PUBLIC
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
    | parenthesizedExpression
    ;

expression
    : (disjunction | command) (BAR command)*
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
    : prefixUnaryExpression (multiplicationOperator prefixUnaryExpression)*
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

prefixUnaryExpression
    : prefixOperator* primaryExpression
    ;

primaryExpression
    : literal
    | VAR_NAME
    | parenthesizedExpression
    | conditionalExpression
    | loopExpression
    ;

parenthesizedExpression
    : OPEN_PARA expression CLOSE_PARA
    ;

conditionExpression
    : ifExpression
    ;

ifExpression
    : IF expression block (ELSE block)?
    ;

loopExpression
    : whileExpression
    ;

whileExpression
    : WHILE expression block
    ;

block
    : OPEN_BRACE statements CLOSE_BRACE
    ;

literal
    : INTEGER
    | SINGLE_QUOTED_STRING
    | doubleQuotedString
    ;

doubleBacktickString
    : DOUBLE_BACKTICK
        ( STRING_SEGMENT(dbt_mode)
        | ESCAPE_SEQUENCE(dbt_mode)
        | escapedExpression(dbt_mode)
        | VAR_NAME(dbt_mode)
      )* DOUBLE_BACKTICK(dbt_mode)
    ;

doubleQuotedString
    : DOUBLE_QUOTE
        ( STRING_SEGMENT(dq_mode)
        | ESCAPE_SEQUENCE(dq_mode)
        | escapedExpression(dq_mode)
        | VAR_NAME(dq_mode)
      )* DOUBLE_QUOTE(dq_mode)
    ;

escapedExpression
    : ESCAPED_EXPRESSION expression(default) CLOSE_PARA(default)
    ;
```

### Informal Grammar Rules

* Prefix operators must exhibit whitespace on the left side and no whitespace on the right side
* Postfix operators must exhibit whitespace on the right side and no whitespace on the left side
* Infix operators must either exhibit whitespace on both sides or on neither side
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
