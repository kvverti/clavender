#ifndef LAVENDER_H
#define LAVENDER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

bool lv_debug;
char* lv_filepath;
char* lv_mainFile;

void lv_run(void);
void lv_repl(void);
void lv_readFile(FILE* file);
void lv_startup(void);
void lv_shutdown(void);
void* lv_alloc(size_t size);
void* lv_realloc(void* ptr, size_t size);
void lv_free(void* ptr);

#endif
