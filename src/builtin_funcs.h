#ifndef BUILTIN_FUNCS_H
#define BUILTIN_FUNCS_H
#include "textbuffer_fwd.h"

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
Intrinsic lv_str_hash;
Intrinsic lv_str_eq;
Intrinsic lv_str_lt;
Intrinsic lv_str_len;
Intrinsic lv_str_slice;

Intrinsic lv_vect_str;
Intrinsic lv_vect_cat;
Intrinsic lv_vect_at;
Intrinsic lv_vect_hash;
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
Intrinsic lv_map_hash;
Intrinsic lv_map_eq;
Intrinsic lv_map_lt;
Intrinsic lv_map_len;
Intrinsic lv_map_map;
Intrinsic lv_map_flatmap;
Intrinsic lv_map_filter;
Intrinsic lv_map_fold;

Intrinsic lv_fun_len;
Intrinsic lv_fun_hash;
Intrinsic lv_fun_eq;
Intrinsic lv_fun_lt;

Intrinsic lv_cap_len;
Intrinsic lv_cap_hash;
Intrinsic lv_cap_eq;
Intrinsic lv_cap_lt;

#endif
