---
title: Functions
---

# Functions
Functions are the basic building block of Lavender programs. In fact,
a Lavender program is nothing more than the evaluation of functions, either
built in or user-defined.

Functions are defined using function expressions, which begin with the keyword
`def`. A given function expression lasts until the next unnested comma
or closing grouper.

## Function Names
Functions may optionally be named. Functions may have alphanumeric or symbolic
names. The type of name has no effect on how the function may be used.

Examples:
```
def func(a) => a
def *(a) => a
def(a) => a
```

### Fixing and Associativity

Function fixing and associativity is specified by adding a prefix to the function
name. Left infix functions use the `i_` prefix, and right associative infix functions
use the `r_` prefix. Regular prefix functions may optionally have the `u_` prefix.
These prefixes are not part of the function name.

```
def u_prefix(a) => a
def i_leftInfix(a, b) => a
def r_rightInfix(a, b) => a
def i_postfix(a) => a
```

**Note**: When right and left associative infix functions are mixed, right
associative functions are a half level of precedence higher than left infix
functions of otherwise equal precedence.
{: .alert .alert-info }

If a left infix function takes only one parameter, it is a postfix function.
Right infix functions cannot be postfix.

### Namespaces and Overloading

If a function is defined at top-level (not inside another function), the user
may refer to it by its simple name, or by its qualified name. A function's qualified
name is the name of its namespace, followed by a colon, followed by the simple name.

```
' --- qual.lv ---
def func(a) => a

' --- REPL ---
@import qual
qual:func(3)
```

*To be continued...*
