#include "textbuffer.h"
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
    #define COPY_NAME(d, s) \
        (d) = lv_alloc(sizeof(BUILTIN_NS s)); \
        memcpy(d, BUILTIN_NS s, sizeof(BUILTIN_NS s))
        
    Operator* op;
    //defined
    op = lv_alloc(sizeof(Operator));
    COPY_NAME(op->name, "defined");
    op->type = FUN_BUILTIN;
    op->arity = 1;
    op->fixing = FIX_PRE;
    op->captureCount = 0;
    op->builtin = defined;
    lv_op_addOperator(op, FNS_PREFIX);
    //add
    op = lv_alloc(sizeof(Operator));
    COPY_NAME(op->name, "__plus__");
    op->type = FUN_BUILTIN;
    op->arity = 2;
    op->fixing = FIX_PRE;
    op->captureCount = 0;
    op->builtin = plus;
    lv_op_addOperator(op, FNS_PREFIX);
    #undef COPY_NAME
    #undef BUILTIN_NS
}

void lv_blt_onShutdown() {
    
}
