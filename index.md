---
title: Index
---
```
def main(args) => "Welcome to Lavender!"
```
Lavender is a pure functional programming language that takes inspiration from
primarily Python and Scala.

## Why Use Lavender?
* **Simplicity** - Lavender relies only on function evaluation, eschewing mutable state
  and complex control flow in favor of simple, declarative function application.
  Conditional primitives are supported via piecewise function definition, or standard
  library functions may be used.
* **Immutability** - Values may *never* be modified, and there are no unsafe functions
  that mutate hidden state. Side effects such as IO and global state are provided via
  standard library abstractions.
* **Clarity** - Lavender aims to have an intuitive syntax and standard library that
  leverage popular notation rather than strict mathematical precision.
* **Referential transparency** - Every terminating expression has a well-defined value
  that may be lazily evaluated or aggressively folded. Lavender supports opt-in lazy
  evaluation through the use of by-name expressions.
* **Locality** - There are no nonlocal control mechanisms in Lavender. Error states
  and branching in general are expressed with recursion and composition.

## Installation

1.  To get started with Lavender, first clone the repository and download the
    Lavender standard libraries.

    ```
    $ git clone https://github.com/kvverti/clavender.git <PROJECT_DIR>
    $ cd <PROJECT_DIR>
    $ git submodule update --init
    ```

    If you have not [set up a GitHub SSH key](https://help.github.com/articles/adding-a-new-ssh-key-to-your-github-account/),
    updating the submodules will not work directly. In this case, you may clone
    the standard libraries directly into the `stdlib` folder.

    ```
    $ git clone https://github.com/kvverti/lavender-stdlib.git <PROJECT_DIR>/stdlib
    ```

2.  Run `make` to compile the project. Building Lavender requires a C compiler
    to be installed. Alternatively, compile the C source files in `src/` using
    the C compiler of your choice. The command below is a minimal example for
    GCC on UNIX.

    ```
    gcc -o lavender -DSTDLIB=\"<PROJECT_DIR>/stdlib/src\" src/*.c
    ```

3.  Assuming the build is successful, run Lavender using the following command.

    ```
    $ ./lavender
    ```

    You should see something like the following.

    ```
    Lavender runtime v. 1.0 by Chris Nero
    Open source at https://github.com/kvverti/clavender
    Enter function definitions or expressions
    >
    ```

    If an error about being unable to find the standard library appears, make
    sure that the `stdlib` subfolder contains Lavender source files.

## Using the Interpreter

Lavender can run programs from files, or run as a REPL (read-eval-print loop).
On the command line, run `./lavender -help` for a list of command line options.

### REPL Mode

If no run file is passed to Lavender, it will open a REPL. In the REPL, the user
can enter expressions or function definitions. Entering an expression will fully
evaluate its value and print the result. Function definitions can be later referred
to by their simple names.

The REPL will normally take a single line of input at a time. However, the REPL
will continue to take lines if there are unclosed parentheses or braces.

Examples:
```
> "hello, world!"
hello, world!
> def func(a) => str(a) ++ " is really neat!"
repl:func
> func("Lavender")
Lavender is really neat!
> (
>    "multiline input" ++
>    " is great"
>    )
multiline input is great
```

### Running Files

Lavender can also evaluate a file passed on the command line. The first non-option
argument to the interpreter is treated as the relative path to the file to run
(sans the `.lv` extension). Any remaining arguments are forwarded to the runtime
as strings, and may be accessed from the running program.
