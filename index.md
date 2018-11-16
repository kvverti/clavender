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
* **Immutability** - Values may *never* be modified, and there are no unsafe functions
  that mutate hidden state.
* **Clarity** - Lavender aims to have an intuitive syntax and standard library that
  leverage popular notation rather than strict mathematical precision.
* **Referential transparency** - Every terminating expression has a well-defined value
  that may be lazily evaluated or aggressively folded.
* **Locality** - There are no nonlocal control mechanisms in Lavender. Error states
  and branching in general are expressed with recursion and composition.

## Getting Started
1.  To get started with Lavender, first clone the repository and download the
    Lavender standard libraries.
    ```
    $ git clone https://github.com/kvverti/clavender.git <PROJECT_DIR>
    $ cd <PROJECT_DIR>
    $ git submodule update
    ```
2.  Run `make` to compile the project. Building Lavender requires a C compiler
    to be installed.

3.  Assuming the build is successful, run Lavender using the following command
    exactly as written.
    ```
    $ ./lavender -fp stdlib/src
    ```
    You should see something like the following.
    ```
    Lavender runtime v. 1.0 by Chris Nero
    Open source at https://github.com/kvverti/clavender
    Enter function definitions or expressions
    >
    ```
    If an error about being unable to find the standard library appears, make
    sure that the `stdlib` subfolder contains Lavender source files and that
    Lavender is being passed the correct file path.
