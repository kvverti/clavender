#include "builtin.h"
#include "builtin_funcs.h"
#include "lavender.h"
#include "builtin_old.c.h"

static Hashtable intrinsics;

Builtin lv_blt_getIntrinsic(char* name) {

    return lv_tbl_get(&intrinsics, name);
}

static Hashtable strFuncs;
static Hashtable vectFuncs;
static Hashtable mapFuncs;
static Hashtable funFuncs;
static Hashtable capFuncs;

static void mkFuncTables(void) {

    lv_tbl_init(&vectFuncs);
    lv_tbl_init(&strFuncs);
    lv_tbl_init(&mapFuncs);
    lv_tbl_init(&funFuncs);
    lv_tbl_init(&capFuncs);
    lv_tbl_put(&vectFuncs, ".str", lv_vect_str);
    lv_tbl_put(&vectFuncs, ".cat", lv_vect_cat);
    lv_tbl_put(&vectFuncs, ".at", lv_vect_at);
    lv_tbl_put(&vectFuncs, ".eq", lv_vect_eq);
    lv_tbl_put(&vectFuncs, ".lt", lv_vect_lt);
    lv_tbl_put(&vectFuncs, ".len", lv_vect_len);
    lv_tbl_put(&vectFuncs, ".map", lv_vect_map);
    lv_tbl_put(&vectFuncs, ".filter", lv_vect_filter);
    lv_tbl_put(&vectFuncs, ".fold", lv_vect_fold);
    lv_tbl_put(&vectFuncs, ".slice", lv_vect_slice);
    lv_tbl_put(&vectFuncs, ".take", lv_vect_take);
    lv_tbl_put(&vectFuncs, ".skip", lv_vect_skip);
    lv_tbl_put(&strFuncs, ".str", lv_str_str);
    lv_tbl_put(&strFuncs, ".num", lv_str_num);
    lv_tbl_put(&strFuncs, ".int", lv_str_int);
    lv_tbl_put(&strFuncs, ".cat", lv_str_cat);
    lv_tbl_put(&strFuncs, ".at", lv_str_at);
    lv_tbl_put(&strFuncs, ".eq", lv_str_eq);
    lv_tbl_put(&strFuncs, ".lt", lv_str_lt);
    lv_tbl_put(&strFuncs, ".len", lv_str_len);
    lv_tbl_put(&strFuncs, ".slice", lv_str_slice);
    lv_tbl_put(&mapFuncs, ".str", lv_map_str);
    lv_tbl_put(&mapFuncs, ".cat", lv_map_cat);
    lv_tbl_put(&mapFuncs, ".at", lv_map_at);
    lv_tbl_put(&mapFuncs, ".eq", lv_map_eq);
    lv_tbl_put(&mapFuncs, ".lt", lv_map_lt);
    lv_tbl_put(&mapFuncs, ".len", lv_map_len);
    lv_tbl_put(&mapFuncs, ".map", lv_map_map);
    lv_tbl_put(&mapFuncs, ".filter", lv_map_filter);
    lv_tbl_put(&mapFuncs, ".fold", lv_map_fold);
    lv_tbl_put(&funFuncs, ".len", lv_fun_len);
    lv_tbl_put(&funFuncs, ".hash", lv_fun_hash);
    lv_tbl_put(&funFuncs, ".eq", lv_fun_eq);
    lv_tbl_put(&funFuncs, ".lt", lv_fun_lt);
    lv_tbl_put(&capFuncs, ".len", lv_cap_len);
    lv_tbl_put(&capFuncs, ".hash", lv_cap_hash);
    lv_tbl_put(&capFuncs, ".eq", lv_cap_eq);
    lv_tbl_put(&capFuncs, ".lt", lv_cap_lt);
}

Hashtable* lv_blt_getFunctionTable(OpType type) {

    switch(type) {
        case OPT_STRING:
            return &strFuncs;
        case OPT_VECT:
            return &vectFuncs;
        case OPT_MAP:
            return &mapFuncs;
        case OPT_FUNCTION_VAL:
            return &funFuncs;
        case OPT_CAPTURE:
            return &capFuncs;
        default:
            return NULL;
    }
}

void lv_blt_onStartup(void) {

    #define SYS "sys:"
    #define MATH "math:"
    #define MK_FUNCT(s, f) lv_tbl_put(&intrinsics, s#f, f)
    #define MK_FUNCN(s, f) lv_tbl_put(&intrinsics, s"__"#f"__", f)
    #define MK_FUNCR(s, f) lv_tbl_put(&intrinsics, s#f, f##_)
    #define MK_FUNNR(s, f) lv_tbl_put(&intrinsics, s"__"#f"__", f##_)
    mkTypes();
    mkFuncTables();
    lv_tbl_init(&intrinsics);
    MK_FUNCT(SYS, defined);
    MK_FUNCT(SYS, undefined);
    MK_FUNCR(SYS, typeof);
    MK_FUNCT(SYS, symb);
    MK_FUNCN(SYS, str);
    MK_FUNCN(SYS, num);
    MK_FUNNR(SYS, int);
    MK_FUNCT(SYS, cval);
    MK_FUNCT(SYS, cat);
    MK_FUNCT(SYS, call);
    MK_FUNCN(SYS, at);
    MK_FUNNR(SYS, bool);
    MK_FUNCN(SYS, hash);
    MK_FUNCN(SYS, eq);
    MK_FUNCN(SYS, lt);
    MK_FUNCN(SYS, ge);
    MK_FUNCN(SYS, add);
    MK_FUNCN(SYS, sub);
    MK_FUNCN(SYS, mul);
    MK_FUNNR(SYS, div);
    MK_FUNCN(SYS, idiv);
    MK_FUNCN(SYS, rem);
    MK_FUNNR(SYS, pow);
    MK_FUNCN(SYS, pos);
    MK_FUNCN(SYS, neg);
    MK_FUNCN(SYS, shl);
    MK_FUNCN(SYS, shr);
    MK_FUNCN(SYS, len);
    MK_FUNCN(SYS, map);
    MK_FUNCN(SYS, filter);
    MK_FUNCN(SYS, fold);
    MK_FUNCN(SYS, slice);
    MK_FUNCN(SYS, concat);
    MK_FUNCN(SYS, take);
    MK_FUNCN(SYS, skip);
    MK_FUNCR(MATH, sin);
    MK_FUNCR(MATH, cos);
    MK_FUNCR(MATH, tan);
    MK_FUNCR(MATH, asin);
    MK_FUNCR(MATH, acos);
    MK_FUNCR(MATH, atan);
    MK_FUNCR(MATH, atan2);
    MK_FUNCR(MATH, sinh);
    MK_FUNCR(MATH, cosh);
    MK_FUNCR(MATH, tanh);
    MK_FUNCR(MATH, exp);
    MK_FUNCR(MATH, log);
    MK_FUNCR(MATH, log10);
    MK_FUNCR(MATH, sqrt);
    MK_FUNCR(MATH, ceil);
    MK_FUNCR(MATH, floor);
    MK_FUNCR(MATH, abs);
    MK_FUNCR(MATH, round);
    MK_FUNCT(MATH, sgn);
    #undef MK_FUNNR
    #undef MK_FUNCR
    #undef MK_FUNCN
    #undef MK_FUNCT
    #undef MATH
    #undef SYS
}

void lv_blt_onShutdown(void) {

    lv_tbl_clear(&intrinsics, NULL);
    lv_free(intrinsics.table);
    for(int i = 0; i < NUM_TYPES; i++)
        lv_free(types[i]);
}
