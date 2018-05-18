#ifndef BUILTIN_H
#define BUILTIN_H
#include "textbuffer.h"

bool lv_blt_toBool(TextBufferObj* obj);

void lv_blt_onStartup();
void lv_blt_onShutdown();

#endif
