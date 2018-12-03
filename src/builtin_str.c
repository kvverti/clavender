#include "builtin_funcs.h"

#define INTRINSIC(name) \
    TextBufferObj lv_str_##name(TextBufferObj* args)
