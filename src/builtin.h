#ifndef BUILTIN_H
#define BUILTIN_H
#include <stdint.h>
#include "textbuffer_fwd.h"

typedef TextBufferObj (*Builtin)(TextBufferObj*);

bool lv_blt_lt(TextBufferObj* a, TextBufferObj* b);
bool lv_blt_equal(TextBufferObj* a, TextBufferObj* b);
uint64_t lv_blt_hash(TextBufferObj* obj);
bool lv_blt_toBool(TextBufferObj* obj);
Builtin lv_blt_getIntrinsic(char* name);

void lv_blt_onStartup(void);
void lv_blt_onShutdown(void);

#endif
