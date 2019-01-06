#ifndef LAVENDER_H
#define LAVENDER_H
#include "textbuffer.h"
#include <stdbool.h>
#include <stddef.h>

#ifndef BUILD
#define BUILD __DATE__ " " __TIME__
#endif

bool lv_debug;
char* lv_filepath;
char* lv_mainFile;
size_t lv_maxStackSize;
struct LvMainArgs {
    char** args;
    int count;
} lv_mainArgs;

// essential non-intrinsic Lavender functions
// they are declared here as they must be initialized after
// Lavender starts up.
TextBufferObj lv_globalEquals;
TextBufferObj lv_globalHash;
TextBufferObj lv_globalLt;

void lv_run(void);
void lv_repl(void);
bool lv_readFile(char* name);
void lv_callFunction(TextBufferObj* func, size_t numArgs, TextBufferObj* args, TextBufferObj* ret);
bool lv_evalByName(TextBufferObj* val, TextBufferObj* ret);
void lv_startup(void);
void lv_shutdown(void);
void* lv_alloc(size_t size);
void* lv_realloc(void* ptr, size_t size);
void lv_free(void* ptr);

#endif
