.RECIPEPREFIX = @

CC = gcc
CSRC = src/*.c integer/src/*.c
STDLIB_DIR = $(CURDIR)/stdlib/src
COMMON_ARGS = -Iinteger/include -Wall -DSTDLIB=\"$(STDLIB_DIR)\" -DBUILD=\"$(shell date -u -Iseconds)\"
RELASE_ARGS = $(COMMON_ARGS) -O3 -DNDEBUG
DEBUG_ARGS = $(COMMON_ARGS) -g

release:
@   $(CC) -o lavender $(RELASE_ARGS) $(CSRC) -lm

debug:
@   $(CC) -o lavender $(DEBUG_ARGS) $(CSRC) -lm
