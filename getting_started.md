---
title: Getting Started
---

**Note**: This section assumes that you have already set up the Lavender interpreter.
If you have not, [download and install Lavender](index.html#installation) first.
{: .alert .alert-info }

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

## Simple Expressions

Expressions make up the bulk of Lavender programs. Expressions are made up of
literal values, collections, and function calls.

There are three types of literal values: numbers, strings, and symbols.

### Numeric Literals

Numeric literals evaluate to integers when consisting solely of a sequence of digits, otherwise
they evaluate as floating-point numbers. Integer literals can be written in base 10,
or in octal, hexadecimal, or binary using the prefixes `0c`, `0x`, or `0b` respectively
An otherwise integer literal can be forced to evaluate as floating-point by appending
the suffix `d`.

Examples:
```
1234    ' 1234
12.34   ' 12.34
1234d   ' 1234.0
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
a pair of curly braces (`{ }`) containing a comma separated list of expressions.

Examples:
```
{ }
{ 1, 2, 3 }
{ "hello", 3 * 2 }
{ "vects can", { "be", "inside" }, "other vects" }
```

### Maps

Maps are Lavender's second collection value. Maps, like vects, are written
enclosed in curly braces (`{ }`). Unlike vects, maps contain a comma separated
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
