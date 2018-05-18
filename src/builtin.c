#include "builtin.h"
#include "lavender.h"
#include <string.h>

/**
 * Returns whether the argument is defined (i.e. not undefined).
 */
static TextBufferObj defined(TextBufferObj* args) {
    
    TextBufferObj res;
    res.type = OPT_NUMBER;
    res.number = (args[0].type != OPT_UNDEFINED);
    return res;
}

/**
 * Returns the undefined value.
 */
static TextBufferObj undefined(TextBufferObj* args) {
    
    TextBufferObj res;
    res.type = OPT_UNDEFINED;
    return res;
}

bool lv_blt_toBool(TextBufferObj* obj) {
    
    return (obj->type != OPT_UNDEFINED)
        && (obj->type != OPT_NUMBER || obj->number != 0.0)
        && (obj->type != OPT_STRING || obj->str->len != 0);
}

/**
 * Converts to bool.
 */
static TextBufferObj bool_(TextBufferObj* args) {
    
    TextBufferObj res;
    res.type = OPT_NUMBER;
    res.number = lv_blt_toBool(&args[0]);
    return res;
}

/**
 * Converts object to string.
 */
static TextBufferObj str(TextBufferObj* args) {
    
    if(args[0].type == OPT_STRING)
        return args[0];
    TextBufferObj res;
    res.type = OPT_STRING;
    res.str = lv_tb_getString(&args[0]);
    return res;
}

/**
 * Addition and string concatenation. Assumes two arguments.
 */
static TextBufferObj plus(TextBufferObj* args) {
    
    TextBufferObj res;
    if(args[0].type == OPT_STRING && args[1].type == OPT_STRING) {
        //string concatenation
        size_t alen = args[0].str->len;
        size_t blen = args[1].str->len;
        LvString* str = lv_alloc(sizeof(LvString) + alen + blen + 1);
        str->len = alen + blen;
        memcpy(str->value, args[0].str->value, alen);
        memcpy(str->value + alen, args[1].str->value, blen);
        str->value[alen + blen] = '\0';
        res.type = OPT_STRING;
        res.str = str;
    } else if(args[0].type == OPT_NUMBER && args[1].type == OPT_NUMBER) {
        //addition
        res.type = OPT_NUMBER;
        res.number = args[0].number + args[1].number;
    } else {
        //undefined
        res.type = OPT_UNDEFINED;
    }
    return res;
}

void lv_blt_onStartup() {
    
    #define BUILTIN_NS "sys:"
    //creates a builtin function with given impl, arity, and name
    #define MK_FUNC_IMPL(fnc, ar, nm) \
        op = lv_alloc(sizeof(Operator)); \
        op->name = lv_alloc(sizeof(BUILTIN_NS nm)); \
        memcpy(op->name, BUILTIN_NS nm, sizeof(BUILTIN_NS nm)); \
        op->type = FUN_BUILTIN; \
        op->arity = ar; \
        op->fixing = FIX_PRE; \
        op->captureCount = 0; \
        op->builtin = fnc; \
        lv_op_addOperator(op, FNS_PREFIX)
    //creates "external" builtin function
    #define MK_FUNC(fnc, ar) MK_FUNC_IMPL(fnc, ar, #fnc)
    //creates "internal" builtin function
    #define MK_FUNCN(fnc, ar) MK_FUNC_IMPL(fnc, ar, "__"#fnc"__")
    Operator* op;
    MK_FUNC(defined, 1);
    MK_FUNC(undefined, 0);
    MK_FUNCN(plus, 2);
    MK_FUNCN(str, 1);
    MK_FUNC_IMPL(bool_, 1, "__bool__");
    //defined
    // op = lv_alloc(sizeof(Operator));
    // COPY_NAME(op->name, "defined");
    // op->type = FUN_BUILTIN;
    // op->arity = 1;
    // op->fixing = FIX_PRE;
    // op->captureCount = 0;
    // op->builtin = defined;
    // lv_op_addOperator(op, FNS_PREFIX);
    //undefined
    // op = lv_alloc(sizeof(Operator));
    // COPY_NAME(op->name, "undefined");
    // op->type = FUN_BUILTIN;
    // op->arity = 0;
    // op->fixing = FIX_PRE;
    // op->captureCount = 0;
    // op->builtin = undefined;
    // lv_op_addOperator(op, FNS_PREFIX);
    //add
    // op = lv_alloc(sizeof(Operator));
    // COPY_NAME(op->name, "__plus__");
    // op->type = FUN_BUILTIN;
    // op->arity = 2;
    // op->fixing = FIX_PRE;
    // op->captureCount = 0;
    // op->builtin = plus;
    // lv_op_addOperator(op, FNS_PREFIX);
    #undef MK_FUNC
    #undef MK_FUNCN
    #undef MK_FUNC_IMPL
    #undef BUILTIN_NS
}

void lv_blt_onShutdown() {
    
}
