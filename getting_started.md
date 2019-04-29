---
title: Program Structure
---

# Program Structure

Lavender files use the extension `.lv`. Each files defines a namespace which contains
public functions and internal functions. Along with runtime commands, these function definitions
are the only expressions that can appear at the top level within a namespace.

A Lavender program must have a function named `main` taking one argument defined
in one of its namespaces. The Lavender interpreter will call this function, passing
command line arguments to it in a vect. The `main` function declaration may take
one of the two forms shown below. The second form takes its arguments by varargs,
but is otherwise equivalent in the eyes of the interpreter.
```
def main(args)
def main(...args)
```

## Comments

Single line comments may be inserted anywhere and are signaled with the single
quote mark (`'`). There are no multiline comments in Lavender.

A shebang line may optionally be present as the first line in a Lavender source
file, where it will be treated as a comment.

Example:
```
#!/usr/some/path/to/lavender

def main(args) => "Hello world!" ' Eval to a string
```

## Simple Expressions

Expressions make up the bulk of Lavender programs. Expressions are made up of
literal values, collections, and function calls.

There are three types of literal values: numbers, strings, and symbols.

### Numeric Literals

Numeric literals evaluate to integers when consisting solely of a sequence of digits, otherwise
they evaluate as floating-point numbers. Integer literals can be written in base 10,
or in octal, hexadecimal, or binary using the prefixes `0c`, `0x`, or `0b` respectively.
An otherwise integer literal can be forced to evaluate as floating-point by appending
the suffix `f` or `d`.

Examples:
```
1234    ' 1234
12.34   ' 12.34
1234f   ' 1234.0
0xff    ' 255
0c777   ' 255
0b111   ' 7
```

**Warning**: Unlike in other languages, Lavender uses an explicit prefix for octal literals.
Numbers prefixed with a `0` (such as `0377`) are parsed as decimal.
{: .alert .alert-warning }

### String Literals

String literals are delineated with double quote marks (`"`), and can contain
any character except for newline characters. Strings can use the following escape
sequences.

Sequence | Meaning
---------|--------
`\n` | New line
`\t` | Tab
`\"` | Double quote
`\'` | Single quote
`\\` | Back slash

Examples:
```
"Hello world"        ' Hello world
"Hello \"world\""    ' Hello "world"
"Escape \\"          ' Escape \
"Hello\nworld"       ' Hello
                     ' world
```

### Symbol Literals

Symbol literals are written using a dot `.` followed by an alphanumeric
name. Symbols can also use quoted names, which can contain any character valid
in strings.

Examples:
```
.symbol
.alpha1234
."quoted name"
."with \"special\" escapes"
```
**Warning**: Symbols beginning with a number must be quoted (`."1"`, not `.1`).
Otherwise, the token will be parsed as a number.
{: .alert .alert-warning }

### Vects

Vectors, or vects, are Lavender's first collection value. Vects are written as
a pair of curly braces (<code>{&nbsp;}</code>) containing a comma separated list of expressions.

Examples:
```
{ }
{ 1, 2, 3 }
{ "hello", 3 * 2 }
{ "vects can", { "be", "inside" }, "other vects" }
```

### Maps

Maps are Lavender's second collection value. Maps, like vects, are written
enclosed in curly braces (<code>{&nbsp;}</code>). Unlike vects, maps contain a comma separated
list of key-value pairs, where an arrow token (`=>`) separates the key from the
value. Keys and values may be any expression. If two keys are equal, the map
contains the value associated with the key that comes textually last.

Examples:
```
{ 1 => 2 }
{ "hello" => "world", .key => "value" }
{ { "vects", "in" } => "maps", "and maps" => { "in" => "maps" } }
```

**Note**: Because vects and maps both use curly braces, the empty map cannot
be written. However, this should not pose a problem because Lavender is untyped.
{: .alert .alert-info }

### Function Values

Function values are written by placing a backslash (`\`) before the function name.
If the function is infix, an additional backslash is placed after the function name.

Function values may also be defined inline using [function expressions](functions.html).

Examples:
```
\len
\+\
def(a) => a + 1
```

## Compound Expressions

Zero or more simple expressions may be combined into compound expressions.
Compound expressions always involve a function call.

### Function Evaluation

A function call is written using the function's name and a list of zero or
more arguments. A single argument may optionally be enclosed in parentheses,
while multiple arguments must always be separated by commas
and enclosed in parentheses. If a function takes zero arguments, then the function
name is written and no argument list is specified. If a function takes a variable
number of arguments, writing an empty pair of parentheses passes zero arguments.

Examples:
```
def f(...a) => ...
def v() => ...
f 1
f(1)
f(1, 2)
f()
v
```

**Warning**: Passing zero arguments to a variable-arity function requires using
an empty pair of parentheses, while evaluating zero-arity functions requires the
absence of parentheses.
{: .alert .alert-warning }

Infix functions are called similarly to prefix functions, except that their
first argument is written to the left of the function name.

### Precedence

When multiple function calls are present in the same expression, precedence is
resolved as follows.

1.  Groups in parentheses.
2.  Prefix functions.
3.  Postfix functions.
4.  Value calls.
5.  Infix functions, where precedence is resolved using the first character
    of the function name.

Infix function precedence is as follows.

Characters | Standard Operators
-----------|-------------------
`~ ?` | `?:`
`**`<sup>1</sup> | `**`
`* / %` | `* / // %`
`+ -` | `+ - ++`
`:` | `::`
`< >` | `< <= >= > << >> <|`
`= !` | `= !=`
`&` | `&& &`
`|` | `|| | |>`
`^` | `^`
`_` and alpha | `map flatmap in filter fold reduce takeWhile skipWhile`
`$` | `$`

1: Infix functions beginning with `**` have precedence over infix functions
    beginning with a single `*`, in order to support the exponentiation
    operator.

### By-Name Expressions
Any expression may be made by-name by prepending it with the arrow token
`=>`. By-name expressions are semantically not evaluated unless their value is needed,
and can therefore be used for recursive definitions, among other things.
The arrow token binds as far as possible, like a function definition, so
by-name subexpressions must be enclosed in a grouper.

```
def f(a) => ...
f(=> 3)  ' --> `3` is not evaluated unless needed
f(=> 3 + 4)  ' --> `3 + 4` is not evaluated unless needed
f => 3  ' --> error: by-name subexpression not enclosed in groupers
{ => 3 + 4, 5 }  ' --> ok: `3 + 4` is enclosed in groupers
```

A by-name expression is evaluated in three cases:
1. It is used as a key in a map.
2. It is called as a function with arguments.
3. It is the return value of a function.

Other uses of by-name expressions, such as using any name bound to them, do
not evaluate the by-name expression.

```
def f(a) => { a, a(0) }
f(=> "hel" ++ "lo")  ' --> { <byname>, h }
```

## Names

Values in Lavender may have one or more names associated with them. Names are
bound to values in function parameters, function locals, and function names.
Function parameter and function local names must be alphanumeric, while function
names may be symbolic.

The symbolic characters allowed in function names are the following.
```
? ~ * / % + - : < > = ! & | ^ $
```

Names may also not be the same as one of the few reserved words.
* `def`
* `let`
* `do`
* `native`
* `=>`
* `<-`
