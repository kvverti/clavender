![Lavender](https://kvverti.github.io/lavender/lavender.svg)
***
Lavender is an interpreted, pure functional REPL-based programming language. It supports built in floating point numbers, strings, and vectors, with lists, hashtables, and functional monads implemented in the standard libraries. Lavender is untyped, and it represents all values as functions. Lavender takes inspiration primarily from C, Python, and Scala.

This repository contains an implementation of Lavender in the C programming language. The original Java implementation is still around for posterity, but may not be updated in the future.

Further documentation can be found on the [Lavender GitHub pages site](https://kvverti.github.io/clavender).

## Running Lavender
After cloning the repository, you can run `make` to build the interpreter. Then, run the interpreter with `./lavender`.

Example:
```
$ git clone https://github.com/kvverti/clavender.git
$ cd ./clavender
$ git submodule update --init
$ make
$ ./lavender
```

There are two options for `make`. The default mode `release` compiles with optimization and without debugging symbols, while `debug` mode compiles without optimization and with debug symbols and assertions intact. The makefile uses `gcc` for compilation.

Lavender accepts the command line options `-fp` to set the library filepath, `-maxStackSize` to set the maximum data stack size, and `-debug` to enable debugging output. Lavender runs in REPL mode by default, where you can enter expressions and see their results. By specifying a file to execute on the command line, Lavender instead executes the file and prints the result to stdout. Note that to access the standard libraries, you must set `-fp` to `stdlib`.

## Goals
The Lavender language is designed with the following ~~restrictions to make things easier~~ goals:
* **Simplicity** - Lavender has very few native constructs. Whenever some functionality can be implemented as a library function, it is.
* **Immutability** - Values in Lavender may never be modified; the concept of a variable does not exist. Constructs such as looping and iteration are expressed using recursion or generators.
* **Referential transparency** - Every terminating expression in Lavender has a well-defined value (even if the value is `undefined`), and this value does not change upon re-evaluation.
* **Locality** - Expressions are always evaluated in-order, nonlocal control mechanisms such as exceptions are not possible.
* **Totality** - Every value is a function, and every function is a value. Values can be meaningfully compared with one another.
