#include "builtin_funcs.h"
#include "textbuffer.h"
#include "operator.h"
#include <assert.h>

#define INTRINSIC(name) \
    TextBufferObj lv_fun_##name(TextBufferObj* args)

INTRINSIC(len) {
    assert(args[0].type == OPT_FUNCTION_VAL);
    TextBufferObj res = {
        .type = OPT_INTEGER,
        .integer = args[0].func->arity
    };
    return res;
}

INTRINSIC(hash) {
    assert(args[0].type == OPT_FUNCTION_VAL);
    TextBufferObj res = {
        .type = OPT_INTEGER,
        .integer = (uint64_t)args[0].func
    };
    return res;
}

INTRINSIC(eq) {
    assert(args[0].type == OPT_FUNCTION_VAL);
    assert(args[1].type == OPT_FUNCTION_VAL);
    TextBufferObj res = {
        .type = OPT_INTEGER,
        .integer = args[0].func == args[1].func
    };
    return res;
}

INTRINSIC(lt) {
    assert(args[0].type == OPT_FUNCTION_VAL);
    assert(args[1].type == OPT_FUNCTION_VAL);
    TextBufferObj res = {
        .type = OPT_INTEGER,
        .integer = (uintptr_t)args[0].func < (uintptr_t)args[1].func
    };
    return res;
}
