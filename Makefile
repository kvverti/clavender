.RECIPEPREFIX = @

CC = gcc
CSRC = src/*.c
RELASE_ARGS = -Wall -O3 -DNDEBUG
DEBUG_ARGS = -Wall -g

release:
@   $(CC) -o lavender $(RELASE_ARGS) $(CSRC) -lm

debug:
@   $(CC) -o lavender $(DEBUG_ARGS) $(CSRC) -lm
