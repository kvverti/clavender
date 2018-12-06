#ifndef BUILTIN_FUNCS_H
#define BUILTIN_FUNCS_H
#include "textbuffer_fwd.h"

void lv_bltf_getArgs(TextBufferObj* dst, TextBufferObj* src, size_t len);
void lv_bltf_clearArgs(TextBufferObj* args, size_t len);

#define INC_REFCOUNT(obj) \
    if((obj)->type & LV_DYNAMIC) \
        ++*(obj)->refCount; \
    else (void)0

#define NEGATIVE_INT(a) ((a) >> 63)

typedef TextBufferObj Intrinsic(TextBufferObj*);

Intrinsic lv_str_str;
Intrinsic lv_str_num;
Intrinsic lv_str_int;
Intrinsic lv_str_cat;
Intrinsic lv_str_at;
Intrinsic lv_str_eq;
Intrinsic lv_str_lt;
Intrinsic lv_str_len;
Intrinsic lv_str_slice;

Intrinsic lv_vect_str;
Intrinsic lv_vect_cat;
Intrinsic lv_vect_at;
Intrinsic lv_vect_eq;
Intrinsic lv_vect_lt;
Intrinsic lv_vect_len;
Intrinsic lv_vect_map;
Intrinsic lv_vect_flatmap;
Intrinsic lv_vect_filter;
Intrinsic lv_vect_fold;
Intrinsic lv_vect_slice;
Intrinsic lv_vect_take;
Intrinsic lv_vect_skip;

Intrinsic lv_map_str;
Intrinsic lv_map_cat;
Intrinsic lv_map_at;
Intrinsic lv_map_eq;
Intrinsic lv_map_lt;
Intrinsic lv_map_len;
Intrinsic lv_map_map;
Intrinsic lv_map_flatmap;
Intrinsic lv_map_filter;
Intrinsic lv_map_fold;

#endif
