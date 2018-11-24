---
title: Getting Started
---

This section assumes that you have already set up the Lavender interpreter.
If you have not, [download and install Lavender](index.html#installation) first.

## Program Structure

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

### Expressions

Expressions make up the bulk of Lavender programs. Expressions are made up of
literal values, collections, and function calls.

There are three types of literal values: numbers, strings, and symbols. Numeric
literals generally evaluate as integers if their value is an integer, otherwise
they evaluate as floating-point numbers. Integer literals can be written in base 10,
or in octal, hexadecimal, or binary using the prefixes `0c`, `0x`, or `0b` respectively
(note that, unlike in other languages, Lavender uses an explicit prefix for octal literals).
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
