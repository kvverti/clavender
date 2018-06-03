#include "builtin.h"
#include "lavender.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>

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

#define NUM_TYPES 4
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
    INIT(3, "function");
    #undef INIT
}

/**
 * Returns the type of this object, as a string.
 * Possible types are: "undefined", "number", "string", "function"
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
        case OPT_FUNCTION_VAL:
            res.str = types[3];
            break;
        default:
            assert(false);
    }
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
            //todo vectors
        default:
            res.type = OPT_UNDEFINED;
    }
    return res;
}

/**
 * Compares two objects for equality.
 */
static TextBufferObj eq(TextBufferObj* args) {
    
    TextBufferObj res;
    res.type = OPT_NUMBER;
    if(args[0].type != args[1].type) {
        //can't be equal if they have different types
        res.number = 0.0;
        return res;
    }
    switch(args[0].type) {
        case OPT_UNDEFINED:
            res.number = 1.0;
            break;
        case OPT_NUMBER:
            res.number = (args[0].number == args[1].number);
            break;
        case OPT_STRING:
            //strings use value equality
            res.number = (args[0].str->len == args[1].str->len)
                && (strcmp(args[0].str->value, args[1].str->value) == 0);
            break;
        case OPT_FUNCTION_VAL:
            //todo capture values
            res.number = (args[0].func == args[1].func);
            break;
        default:
            res.number = 0.0;
    }
    return res;
}

/**
 * Compares two objects for less than.
 */
static bool ltImpl(TextBufferObj* args) {
    
    if(args[0].type != args[1].type) {
        return (args[0].type < args[1].type);
    }
    switch(args[0].type) {
        case OPT_UNDEFINED:
            return false;
            break;
        case OPT_NUMBER:
            return (args[0].number < args[1].number);
            break;
        case OPT_STRING:
            return (strcmp(args[0].str->value, args[1].str->value) < 0);
            break;
        case OPT_FUNCTION_VAL:
            //todo capture values
            return (strcmp(args[0].func->name, args[1].func->name) < 0);
            break;
        default:
            return false;
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
    res.number = ltImpl(args);
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
    res.number = !ltImpl(args);
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
    Operator* op;
    MK_FUNC(defined, 1);
    MK_FUNC(undefined, 0);
    MK_FUNC_IMPL(typeof_, 1, "typeof");
    MK_FUNCN(str, 1);
    MK_FUNCN(num, 1);
    MK_FUNC_IMPL(bool_, 1, "__bool__");
    MK_FUNCN(eq, 2);
    MK_FUNCN(lt, 2);
    MK_FUNCN(ge, 2);
    MK_FUNCN(add, 2);
    MK_FUNCN(sub, 2);
    MK_FUNCN(mul, 2);
    MK_FUNC_IMPL(div_, 2, "__div__");
    MK_FUNCN(len, 1);
    #undef MK_FUNC
    #undef MK_FUNCN
    #undef MK_FUNC_IMPL
    #undef BUILTIN_NS
}

void lv_blt_onShutdown(void) {
    
    for(int i = 0; i < NUM_TYPES; i++)
        lv_free(types[i]);
}
