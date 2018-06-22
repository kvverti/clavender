![Lavender](https://kvverti.github.io/lavender/lavender.svg)
***
Lavender is an interpreted, pure functional REPL-based programming language. It supports built in floating point numbers, strings, and vectors, with lists, hashtables, and functional monads implemented in the standard libraries. Lavender is untyped, and it represents all values as functions. Lavender takes inspiration primarily from C, Python, and Scala.

This repository contains an implementation of Lavender in the C programming language. The original Java implementation is still around for posterity, but may not be updated in the future.

## Running Lavender
After cloning the repository, you can run `make` to build the interpreter. Then, run the interpreter with `./lavender`.

Example:
```
$ git clone https://github.com/kvverti/clavender.git
$ cd ./clavender
$ make
$ ./lavender
```

There are two options for `make`. The default mode `release` compiles with optimization and without debugging symbols, while `debug` mode compiles without optimization and with debug symbols and assertions intact. The makefile uses `gcc` for compilation.

Lavender accepts the command line options `-fp` to set the library filepath, `-maxStackSize` to set the maximum data stack size, and `-debug` to enable debugging output. Lavender runs in REPL mode by default, where you can enter expressions and see their results. By specifying a file to execute on the command line, Lavender instead executes the file and prints the result to stdout. Note that to access the standard libraries, you must set `-fp` to `stdlib`.

Further documentation can be found on the [GitHub pages site](https://kvverti.github.io/lavender).
