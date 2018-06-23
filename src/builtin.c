#include "builtin.h"
#include "lavender.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

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

#define NUM_TYPES 5
static LvString* types[NUM_TYPES];

static void mkTypes(void) {
    
    #define INIT(i, n) \
        types[i] = lv_alloc(sizeof(LvString) + sizeof(n)); \
        types[i]->len = sizeof(n) - 1; \
        types[i]->refCount = 1; \
        memcpy(types[i]->value, n, sizeof(n))
    INIT(0, "undefined");
    INIT(1, "number");
    INIT(2, "string");
    INIT(3, "vect");
    INIT(4, "function");
    #undef INIT
}

/**
 * Returns the type of this object, as a string.
 * Possible types are: "undefined", "number", "string", "vect", "function"
 */
static TextBufferObj typeof_(TextBufferObj* args) {
    
    TextBufferObj res;
    res.type = OPT_STRING;
    switch(args[0].type) {
        case OPT_UNDEFINED:
            res.str = types[0];
            break;
        case OPT_NUMBER:
            res.str = types[1];
            break;
        case OPT_STRING:
            res.str = types[2];
            break;
        case OPT_VECT:
            res.str = types[3];
        case OPT_CAPTURE:
        case OPT_FUNCTION_VAL:
            res.str = types[4];
            break;
        default:
            assert(false);
    }
    return res;
}

/**
 * Gets the n'th capture value from the given function.
 */
static TextBufferObj cval(TextBufferObj* args) {
    
    TextBufferObj res;
    if((args[0].type == OPT_CAPTURE)
    && (args[1].type == OPT_NUMBER)
    && (args[1].number >= 0 && args[1].number < args[0].capfunc->captureCount)) {
        res = args[0].capture->value[(size_t)args[1].number];
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

bool lv_blt_toBool(TextBufferObj* obj) {
    
    return (obj->type != OPT_UNDEFINED)
        && (obj->type != OPT_NUMBER || obj->number != 0.0)
        && (obj->type != OPT_STRING || obj->str->len != 0)
        && (obj->type != OPT_VECT || obj->veclen != 0);
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

/** Converts object to number. */
static TextBufferObj num(TextBufferObj* args) {
    
    TextBufferObj res;
    if(args[0].type == OPT_NUMBER)
        return args[0];
    else if(args[0].type == OPT_STRING) {
        char* rest;
        LvString* str = args[0].str;
        double d = strtod(str->value, &rest);
        if(rest != str->value + str->len) {
            //not all chars interpreted, error
            res.type = OPT_UNDEFINED;
        } else {
            res.type = OPT_NUMBER;
            res.number = d;
        }
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

/**
 * Returns length of object if defined.
 */
static TextBufferObj len(TextBufferObj* args) {
    
    TextBufferObj res;
    switch(args[0].type) {
        case OPT_STRING:
            //length of string
            res.type = OPT_NUMBER;
            res.number = args[0].str->len;
            break;
        case OPT_FUNCTION_VAL:
            //arity of function
            res.type = OPT_NUMBER;
            res.number = args[0].func->arity;
            break;
        case OPT_CAPTURE:
            res.type = OPT_NUMBER;
            res.number = args[0].capfunc->arity - args[0].capfunc->captureCount;
            break;
        case OPT_VECT:
            res.type = OPT_NUMBER;
            res.number = args[0].veclen;
            break;
        default:
            res.type = OPT_UNDEFINED;
    }
    return res;
}

static bool equal(TextBufferObj* a, TextBufferObj* b) {
    
    if(a->type != b->type) {
        //can't be equal if they have different types
        return false;
    }
    switch(a->type) {
        case OPT_UNDEFINED:
            return true;
        case OPT_NUMBER:
            return a->number == b->number;
        case OPT_STRING:
            //strings use value equality
            return (a->str->len == a->str->len)
                && (strcmp(a->str->value, b->str->value) == 0);
        case OPT_FUNCTION_VAL:
            return a->func == b->func;
        case OPT_CAPTURE:
            if(a->capfunc != b->capfunc)
                return false;
            for(int i = 0; i < a->capfunc->captureCount; i++) {
                if(!equal(&a->capture->value[i], &b->capture->value[i]))
                    return false;
            }
            return true;
        case OPT_VECT:
            if(a->veclen != b->veclen)
                return false;
            for(size_t i = 0; i < a->veclen; i++) {
                if(!equal(&a->vecdata[i], &b->vecdata[i]))
                    return false;
            }
            return true;
        default:
            assert(false);
    }
}

/**
 * Compares two objects for equality.
 */
static TextBufferObj eq(TextBufferObj* args) {
    
    TextBufferObj res;
    res.type = OPT_NUMBER;
    res.number = equal(&args[0], &args[1]);
    return res;
}

/**
 * Compares two objects for less than.
 */
static bool ltImpl(TextBufferObj* a, TextBufferObj* b) {
    
    if(a->type != b->type) {
        return (a->type < b->type);
    }
    switch(a->type) {
        case OPT_UNDEFINED:
            return false;
            break;
        case OPT_NUMBER:
            return (a->number < b->number);
            break;
        case OPT_STRING:
            return (strcmp(a->str->value, b->str->value) < 0);
            break;
        case OPT_FUNCTION_VAL:
            return (uintptr_t)a->func < (uintptr_t)b->func;
            break;
        //captures and vects compare the first nonequal values
        case OPT_CAPTURE:
            if(a->capfunc == b->capfunc) {
                for(int i = 0; i < a->capfunc->captureCount; i++) {
                    if(!equal(&a->capture->value[i], &b->capture->value[i]))
                        return ltImpl(&a->capture->value[i], &b->capture->value[i]);
                }
                return false;
            }
            return (uintptr_t)a->capfunc < (uintptr_t)b->capfunc;
        case OPT_VECT:
            if(a->veclen == b->veclen) {
                for(size_t i = 0; i < a->veclen; i++) {
                    if(!equal(&a->vecdata[i], &b->vecdata[i]))
                        return ltImpl(&a->vecdata[i], &b->vecdata[i]);
                }
                return false;
            }
            return a->veclen < b->veclen;
        default:
            assert(false);
    }
}

static TextBufferObj lt(TextBufferObj* args) {
    
    TextBufferObj res;
    res.type = OPT_NUMBER;
    if((args[0].type == OPT_NUMBER && args[1].type == OPT_NUMBER)
    && ((args[0].number != args[0].number) || (args[1].number != args[1].number))) {
        //one of them is NaN
        res.number = 0.0;
        return res;
    }
    res.number = ltImpl(&args[0], &args[1]);
    return res;
}

//we need both lt and ge because NaN always compares false.
//the Lavender comparison functions use either lt or ge
//as appropriate.
static TextBufferObj ge(TextBufferObj* args) {
    
    TextBufferObj res;
    res.type = OPT_NUMBER;
    if((args[0].type == OPT_NUMBER && args[1].type == OPT_NUMBER)
    && ((args[0].number != args[0].number) || (args[1].number != args[1].number))) {
        //one of them is NaN
        res.number = 0.0;
        return res;
    }
    res.number = !ltImpl(&args[0], &args[1]);
    return res;
}

/**
 * Addition and string concatenation. Assumes two arguments.
 */
static TextBufferObj add(TextBufferObj* args) {
    
    TextBufferObj res;
    if(args[0].type == OPT_STRING && args[1].type == OPT_STRING) {
        //string concatenation
        size_t alen = args[0].str->len;
        size_t blen = args[1].str->len;
        LvString* str = lv_alloc(sizeof(LvString) + alen + blen + 1);
        str->refCount = 0;
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

/**
 * Subtraction. Assumes two arguments.
 */
static TextBufferObj sub(TextBufferObj* args) {
    
    TextBufferObj res;
    if(args[0].type == OPT_NUMBER && args[1].type == OPT_NUMBER) {
        res.type = OPT_NUMBER;
        res.number = args[0].number - args[1].number;
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

/** Multiplication */
static TextBufferObj mul(TextBufferObj* args) {
    
    TextBufferObj res;
    if(args[0].type == OPT_NUMBER && args[1].type == OPT_NUMBER) {
        res.type = OPT_NUMBER;
        res.number = args[0].number * args[1].number;
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

/** Division */
static TextBufferObj div_(TextBufferObj* args) {
    
    TextBufferObj res;
    if(args[0].type == OPT_NUMBER && args[1].type == OPT_NUMBER) {
        res.type = OPT_NUMBER;
        res.number = args[0].number / args[1].number;
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

/** Exponentiation */
static TextBufferObj pow_(TextBufferObj* args) {
    
    TextBufferObj res;
    if(args[0].type == OPT_NUMBER && args[1].type == OPT_NUMBER) {
        res.type = OPT_NUMBER;
        res.number = pow(args[0].number, args[1].number);
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

/** Unary + function */
static TextBufferObj pos(TextBufferObj* args) {
    
    TextBufferObj res;
    if(args[0].type == OPT_NUMBER) {
        res.type = OPT_NUMBER;
        res.number = +args[0].number;
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

/** Negation function */
static TextBufferObj neg(TextBufferObj* args) {
    
    TextBufferObj res;
    if(args[0].type == OPT_NUMBER) {
        res.type = OPT_NUMBER;
        res.number = -args[0].number;
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

void lv_blt_onStartup(void) {
    
    mkTypes();
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
    //creates a function that happens to have a C reserved id as a name
    #define MK_FUNCR(fnc, ar) MK_FUNC_IMPL(fnc##_, ar, "__"#fnc"__")
    Operator* op;
    MK_FUNC(defined, 1);
    MK_FUNC(undefined, 0);
    MK_FUNC_IMPL(typeof_, 1, "typeof");
    MK_FUNCN(str, 1);
    MK_FUNCN(num, 1);
    MK_FUNCN(cval, 2);
    MK_FUNCR(bool, 1);
    MK_FUNCN(eq, 2);
    MK_FUNCN(lt, 2);
    MK_FUNCN(ge, 2);
    MK_FUNCN(add, 2);
    MK_FUNCN(sub, 2);
    MK_FUNCN(mul, 2);
    MK_FUNCR(div, 2);
    MK_FUNCR(pow, 2);
    MK_FUNCN(pos, 1);
    MK_FUNCN(neg, 1);
    MK_FUNCN(len, 1);
    #undef MK_FUNC
    #undef MK_FUNCN
    #undef MK_FUNCR
    #undef MK_FUNC_IMPL
    #undef BUILTIN_NS
}

void lv_blt_onShutdown(void) {
    
    for(int i = 0; i < NUM_TYPES; i++)
        lv_free(types[i]);
}
