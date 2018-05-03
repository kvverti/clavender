#ifndef LAVENDER_H
#define LAVENDER_H
#include <stdbool.h>
#include <stddef.h>

bool lv_debug;
char* lv_filepath;
char* lv_mainFile;

void lv_run();
void lv_repl();
void lv_shutdown();
void* lv_alloc(size_t size);

#endif