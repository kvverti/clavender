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

Prefix and infix functions within a namespace may have the same name. An example
from the standard library is the `-` function. This is not recommended in general, however.

```
def -(a) => negation
def i_-(a, b) => subtraction
```

## Parameters and Locals
Functions may take zero or more parameters. Parameters may be either normal
parameters or by-name parameters. When calling a function that takes parameters
by-name, the arguments corresponding to these parameters are implicitly by-name.

```
def f(a, b) => ...  ' Binary function
def g(a, => b, c) => ...  ' Ternary function whose second parameter is by-name
g(1 + 2, 3 + 4, 5 + 6)  ' `3 + 4` is implicitly by-name
```

### Varargs
The last parameter to a function may optionally be a varargs parameter, regardless
of its by-name status. When calling such a function, any number of arguments (including
zero) may be passed that correspond to this parameter. The arguments are wrapped
in a `vect` when passed to the function. If the varargs parameter is by-name, each
corresponding argument is implicitly by-name.

```
def f(a, ...b) => ...
f(1)  ' b is {}
f(1, 2)  ' b is { 2 }
f(1, 2, 3)  ' b is { 2, 3 }
```

### Function Locals
Functions may define a list of local values using the `let` keyword. These values
are initialized with an arbitrary expression, then are accessible to the remainder
of the function. Function local values may access parameters and locals defined
previously, but may not refer to future locals nor themselves. A function defined
in a local value captures only the locals defined before it.

```
def f(a)
    let b(a), c(b + 1) => ...
def f(a)
    let b(c), c(b) => ...  ' error: b cannot refer to c
def f(a)
    let b(a + 1), c(def(d) => ...) => ...  ' c captures a and b only
```

## Nested Functions
Functions may be defined within other functions. Such a function is a nested
function. Nested functions capture values bound in the enclosing function(s), which
refer to the enclosing function's arguments, as expected. Nested function definitions
bind as far as possible, and so should be enclosed in groupers if they appear in
the middle of the outer function definition.

```
def f(a) => def g(b) => { a, b }
f(1)  ' --> repl:f:g[1] (g captures value `1`)
f(1)(2)  ' --> { 1, 2 }
```

## Piecewise Functions
Functions may be defined piecewise, where different function bodies are evaluated
under certain conditions. Each body begins with the arrow token `=>` and the condition
is separated from the body using the semicolon `;`. The body corresponding to the
first condition that is truthy is evaluated as the value of the function.

```
(def f(a)
    => a + 1 ; a > 0
    => a - 1 ; a < 0
    => 42 ; a = 0
)
```

## Calling Values
Lavender statically checks written function calls for correct arity. However,
you can also call arbitrary values, which Lavender checks for validity at runtime.
These arbitrary values may be literals, names, or any expression.

```
def f(a) => a(2, 3)
"hello world"(4)  ' --> o
f(\+\)   ' --> 5
f(\+)    ' --> <undefined>
```

Calling values uses the same syntax as calling functions. Notably, parentheses
may be omitted around a single argument.

**Warning**: Using overloaded functions in value calls without parentheses may
produce ambiguous expressions. In these cases, the expression is parsed as a
non-value call. For example, the expression <code>a&nbsp;-&nbsp;2</code> is parsed
as subtracting `2`from `a`, not as calling `a` with the value `-2`, if `a` is a value.
{: .alert .alert-warning }
