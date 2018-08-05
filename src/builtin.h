#ifndef BUILTIN_H
#define BUILTIN_H
#include "textbuffer_fwd.h"

bool lv_blt_toBool(TextBufferObj* obj);

void lv_blt_onStartup(void);
void lv_blt_onShutdown(void);

#endif
