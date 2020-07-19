#ifndef BUILTIN_H
#define BUILTIN_H
#include <stdint.h>
#include "textbuffer_fwd.h"
#include "hashtable.h"

typedef TextBufferObj (*Builtin)(TextBufferObj*);

LvString* lv_blt_str(TextBufferObj* obj);
bool lv_blt_lt(TextBufferObj* a, TextBufferObj* b);
bool lv_blt_equal(TextBufferObj* a, TextBufferObj* b);
uint64_t lv_blt_hash(TextBufferObj* obj);
bool lv_blt_toBool(TextBufferObj* obj);
Builtin lv_blt_getIntrinsic(char* name);
Hashtable* lv_blt_getFunctionTable(OpType type);

/**
 * Replace by-name expression arguments with their actual results,
 * in place.
 */
void getActualArgs(TextBufferObj* args, size_t len);

void lv_blt_onStartup(void);
void lv_blt_onShutdown(void);

#endif
