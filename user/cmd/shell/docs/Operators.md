# Operators

The Serena shell supports a number of mathematical and logical operators. The following table lists the supported infix operators:

| Operator | Description |
|----------|-------------|
| +        | Addition, string concatenation |
| -        | Subtraction |
| *        | Multiplication |
| /        | Division |
| %        | Modulo |
| ==       | Equals |
| !=       | Not equals |
| <        | Less than |
| <=       | Less than or equals |
| >        | Greater than |
| >=       | Greater than or equals |
| &#124;&#124; | Logical or |
| &&       | Logical and |
| &#124;   | Pipeline connector |
| =        | Assignment |


The following prefix operators are supported:

| Operator | Description |
|----------|-------------|
| +        | Positive number |
| -        | Negative number |
| !        | Logical not |


The following table lists each operator in descending order of precedence.

| Precedence | Operator |
|----------|-------------|
| 0        | !, + (prefix), - (prefix) |
| 1        | *, /, % |
| 2        | +, - |
| 3        | ==, !=, <, <=, >, >= |
| 4        | && |
| 5        | &#124;&#124; |
| 6        | &#124; |
| 7        | = |
