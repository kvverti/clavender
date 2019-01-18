.RECIPEPREFIX = @

CC ?= gcc
CSRC = src/*.c integer/src/*.c
STDLIB_DIR = $(CURDIR)/stdlib/src
BUILD_ID = $(shell git rev-parse HEAD)
COMMON_ARGS = -o lavender -Iinteger/include -Isrc -Wall -DSTDLIB=\"$(STDLIB_DIR)\"
RELASE_ARGS = $(COMMON_ARGS) -O3 -DNDEBUG
DEBUG_ARGS = $(COMMON_ARGS) -g

release:
@   $(CC) -DBUILD=\"$(BUILD_ID)\" $(RELASE_ARGS) $(CSRC) -lm

debug:
@   $(CC) -DBUILD=\"$(BUILD_ID)-debug\" $(DEBUG_ARGS) $(CSRC) -lm
