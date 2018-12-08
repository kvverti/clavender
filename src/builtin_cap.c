#include "builtin_funcs.h"
#include "builtin.h"
#include "textbuffer.h"
#include "operator.h"
#include <assert.h>

#define INTRINSIC(name) \
    TextBufferObj lv_cap_##name(TextBufferObj* args)

INTRINSIC(len) {
    assert(args[0].type == OPT_CAPTURE);
    TextBufferObj res = {
        .type = OPT_INTEGER,
        .integer = args[0].capfunc->arity - args[0].capfunc->captureCount
    };
    return res;
}

INTRINSIC(hash) {
    assert(args[0].type == OPT_CAPTURE);
    TextBufferObj* captures = args[0].capture->value;
    size_t captureCount = args[0].capfunc->captureCount;
    uint64_t h = (uint64_t)args[0].capfunc;
    for(size_t i = 0; i < captureCount; i++) {
        h = ((h << 5) + h) + lv_blt_hash(&captures[i]);
    }
    TextBufferObj res = {
        .type = OPT_INTEGER,
        .integer = h
    };
    return res;
}

INTRINSIC(eq) {
    assert(args[0].type == OPT_CAPTURE);
    assert(args[1].type == OPT_CAPTURE);
    bool eq = true;
    if(args[0].capfunc != args[1].capfunc) {
        eq = false;
    } else {
        size_t captureCount = args[0].capfunc->captureCount;
        TextBufferObj* a = args[0].capture->value;
        TextBufferObj* b = args[1].capture->value;
        for(int i = 0; eq && i < captureCount; i++) {
            if(!lv_blt_equal(&a[i], &b[i]))
                eq = false;
        }
    }
    TextBufferObj res = {
        .type = OPT_INTEGER,
        .integer = eq
    };
    return res;
}

INTRINSIC(lt) {
    assert(args[0].type == OPT_CAPTURE);
    assert(args[1].type == OPT_CAPTURE);
    bool lt = false;
    if(args[0].capfunc == args[1].capfunc) {
        size_t captureCount = args[0].capfunc->captureCount;
        TextBufferObj* a = args[0].capture->value;
        TextBufferObj* b = args[1].capture->value;
        for(int i = 0; i < captureCount; i++) {
            if(lv_blt_lt(&a[i], &b[i])) {
                lt = true;
                break;
            } else if(lv_blt_lt(&b[i], &a[i])) {
                lt = false;
                break;
            }
        }
    } else {
        lt = (uintptr_t)args[0].capfunc < (uintptr_t)args[1].capfunc;
    }
    TextBufferObj res = {
        .type = OPT_INTEGER,
        .integer = lt
    };
    return res;
}
