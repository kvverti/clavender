.RECIPEPREFIX = @

CC = gcc
CSRC = src/*.c
RELASE_ARGS = -Wall -O3 -DNDEBUG
DEBUG_ARGS = -Wall -g
STDLIB_DIR = $(CURDIR)/stdlib/src

release:
@   $(CC) -o lavender -DSTDLIB=\"$(STDLIB_DIR)\" $(RELASE_ARGS) $(CSRC) -lm

debug:
@   $(CC) -o lavender -DSTDLIB=\"$(STDLIB_DIR)\" $(DEBUG_ARGS) $(CSRC) -lm
