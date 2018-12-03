#include "builtin_funcs.h"
#include "lavender.h"
#include "expression.h"

// evaluates any by-name expressions in the arguments
void lv_bltf_getArgs(TextBufferObj* dst, TextBufferObj* src, size_t len) {

    for(size_t i = 0; i < len; i++) {
        if(!lv_evalByName(&src[i], &dst[i])) {
            dst[i] = src[i];
        }
        if(dst[i].type & LV_DYNAMIC) {
            ++*dst[i].refCount;
        }
    }
}

void lv_bltf_clearArgs(TextBufferObj* args, size_t len) {

    lv_expr_cleanup(args, len);
}
