# Grammar

This chapter defines the formal and informal grammar rules of the language.

## Formal Grammar

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
    | PERCENT
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
    : prefixUnaryExpression (multiplicationOperator prefixUnaryExpression)*
    ;

multiplicationOperator
    : ASTERISK
    | PERCENT
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

## Informal Grammar Rules

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
