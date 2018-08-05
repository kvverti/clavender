#include "builtin.h"
#include "lavender.h"
#include "expression.h"
#include "operator.h"
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
            break;
        case OPT_CAPTURE:
        case OPT_FUNCTION_VAL:
            res.str = types[4];
            break;
        default:
            assert(false);
    }
    return res;
}

static void incRefCount(TextBufferObj* obj) {

    if(obj->type & LV_DYNAMIC)
        ++*obj->refCount;
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

/**
 * Flattens the given vectors and elements into a single
 * vector of elements.
 */
static TextBufferObj cat(TextBufferObj* args) {

    TextBufferObj res;
    res.type = OPT_VECT;
    assert(args[0].type == OPT_VECT);
    size_t len = 0;
    for(size_t i = 0; i < args[0].vect->len; i++) {
        TextBufferObj* obj = &args[0].vect->data[i];
        len += obj->type == OPT_VECT ? obj->vect->len : 1;
    }
    res.vect = lv_alloc(sizeof(LvVect) + len * sizeof(TextBufferObj));
    res.vect->refCount = 0;
    res.vect->len = len;
    size_t idx = 0;
    for(size_t i = 0; i < args[0].vect->len; i++) {
        TextBufferObj* obj = &args[0].vect->data[i];
        if(obj->type == OPT_VECT) {
            for(int j = 0; j < obj->vect->len; j++) {
                incRefCount(&obj->vect->data[j]);
                res.vect->data[idx++] = obj->vect->data[j];
            }
        } else {
            incRefCount(obj);
            res.vect->data[idx++] = *obj;
        }
    }
    assert(idx == len);
    return res;
}

/**
 * Calls the given function with the given vector of
 * arguments. Varargs functions have extra parameters
 * packed into the varargs vector.
 */
static TextBufferObj call(TextBufferObj* args) {

    TextBufferObj res;
    TextBufferObj func = args[0];
    if(args[1].type != OPT_VECT) {
        res.type = OPT_UNDEFINED;
    } else {
        lv_callFunction(&func, args[1].vect->len, args[1].vect->data, &res);
    }
    return res;
}

/**
 * Returns the i'th element of the given string or vect.
 */
static TextBufferObj at(TextBufferObj* args) {

    TextBufferObj res;
    if(args[0].type == OPT_NUMBER) {
        if(args[1].type == OPT_STRING
        && args[0].number >= 0 && args[0].number < args[1].str->len) {
            res.type = OPT_STRING;
            res.str = lv_alloc(sizeof(LvString) + 2);
            res.str->refCount = 0;
            res.str->len = 1;
            res.str->value[0] = args[1].str->value[(size_t)args[0].number];
            res.str->value[1] = '\0';
        } else if(args[1].type == OPT_VECT
            && args[0].number >= 0 && args[0].number < args[1].vect->len) {
            res = args[1].vect->data[(size_t)args[0].number];
        } else {
            res.type = OPT_UNDEFINED;
        }
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

bool lv_blt_toBool(TextBufferObj* obj) {

    return (obj->type != OPT_UNDEFINED)
        && (obj->type != OPT_NUMBER || obj->number != 0.0)
        && (obj->type != OPT_STRING || obj->str->len != 0)
        && (obj->type != OPT_VECT || obj->vect->len != 0);
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
            res.number = args[0].vect->len;
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
            if(a->vect->len != b->vect->len)
                return false;
            for(size_t i = 0; i < a->vect->len; i++) {
                if(!equal(&a->vect->data[i], &b->vect->data[i]))
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
            if(a->vect->len == b->vect->len) {
                for(size_t i = 0; i < a->vect->len; i++) {
                    if(!equal(&a->vect->data[i], &b->vect->data[i]))
                        return ltImpl(&a->vect->data[i], &b->vect->data[i]);
                }
                return false;
            }
            return a->vect->len < b->vect->len;
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
        if(args[1].number == 0.0) {
            double sign = copysign(1.0, args[1].number)
                * copysign(1.0, args[0].number);
            double ans = args[0].number == 0.0 ? NAN : INFINITY;
            ans = copysign(ans, sign);
            res.number = ans;
        } else {
            res.number = args[0].number / args[1].number;
        }
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

/** Remainder */
static TextBufferObj rem(TextBufferObj* args) {

    TextBufferObj res;
    if(args[0].type == OPT_NUMBER && args[1].type == OPT_NUMBER) {
        res.type = OPT_NUMBER;
        if(args[1].number == 0.0) {
            double sign = copysign(1.0, args[1].number)
                * copysign(1.0, args[0].number);
            double ans = args[0].number == 0.0 ? NAN : INFINITY;
            ans = copysign(ans, sign);
            res.number = ans;
        } else {
            res.number = fmod(args[0].number, args[1].number);
        }
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

#define DECL_MATH_FUNC(fnc) \
static TextBufferObj fnc##_(TextBufferObj* args) { \
    TextBufferObj res; \
    if(args[0].type == OPT_NUMBER) { \
        res.type = OPT_NUMBER; \
        res.number = fnc(args[0].number); \
    } else { \
        res.type = OPT_UNDEFINED; \
    } \
    return res; \
}

/* Math functions */
DECL_MATH_FUNC(sin);
DECL_MATH_FUNC(cos);
DECL_MATH_FUNC(tan);
DECL_MATH_FUNC(asin);
DECL_MATH_FUNC(acos);
DECL_MATH_FUNC(atan);
DECL_MATH_FUNC(sinh);
DECL_MATH_FUNC(cosh);
DECL_MATH_FUNC(tanh);
DECL_MATH_FUNC(exp);
DECL_MATH_FUNC(log);
DECL_MATH_FUNC(log10);
DECL_MATH_FUNC(sqrt);
DECL_MATH_FUNC(ceil);
DECL_MATH_FUNC(floor);
DECL_MATH_FUNC(fabs);
DECL_MATH_FUNC(round);

#undef DECL_MATH_FUNC

static TextBufferObj atan2_(TextBufferObj* args) {

    TextBufferObj res;
    if(args[0].type == OPT_NUMBER && args[1].type == OPT_NUMBER) {
        res.type = OPT_NUMBER;
        res.number = atan2(args[0].number, args[1].number);
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

/** Sign function. Returns +1 for +0 and -1 for -0. */
static TextBufferObj sgn(TextBufferObj* args) {

    TextBufferObj res;
    if(args[0].type == OPT_NUMBER) {
        res.type = OPT_NUMBER;
        res.number = copysign(1.0, args[0].number);
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

//functional functions

/** Functional map */
static TextBufferObj map(TextBufferObj* args) {

    TextBufferObj res;
    if(args[0].type == OPT_VECT) {
        TextBufferObj func = args[1]; //in case the stack is reallocated
        TextBufferObj* oldData = args[0].vect->data;
        size_t len = args[0].vect->len;
        LvVect* vect = lv_alloc(sizeof(LvVect) + len * sizeof(TextBufferObj));
        vect->refCount = 0;
        vect->len = len;
        for(size_t i = 0; i < len; i++) {
            TextBufferObj obj;
            lv_callFunction(&func, 1, &oldData[i], &obj);
            incRefCount(&obj);
            vect->data[i] = obj;
        }
        res.type = OPT_VECT;
        res.vect = vect;
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

/** Functional filter */
static TextBufferObj filter(TextBufferObj* args) {

    TextBufferObj res;
    if(args[0].type == OPT_VECT) {
        TextBufferObj func = args[1];
        TextBufferObj* oldData = args[0].vect->data;
        size_t len = args[0].vect->len;
        LvVect* vect = lv_alloc(sizeof(LvVect) + len * sizeof(TextBufferObj));
        vect->refCount = 0;
        size_t newLen = 0;
        for(size_t i = 0; i < len; i++) {
            TextBufferObj passed;
            lv_callFunction(&func, 1, &oldData[i], &passed);
            incRefCount(&passed); //so lv_expr_cleanup doesn't blow up
            if(lv_blt_toBool(&passed)) {
                incRefCount(&oldData[i]);
                vect->data[newLen] = oldData[i];
                newLen++;
            }
            lv_expr_cleanup(&passed, 1);
        }
        vect->len = newLen;
        vect = lv_realloc(vect, sizeof(LvVect) + newLen * sizeof(TextBufferObj));
        res.type = OPT_VECT;
        res.vect = vect;
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

/** Functional fold */
static TextBufferObj fold(TextBufferObj* args) {

    TextBufferObj res;
    if(args[0].type == OPT_VECT) {
        size_t len = args[0].vect->len;
        TextBufferObj* oldData = args[0].vect->data;
        TextBufferObj accum[2] = { args[1] };
        TextBufferObj func = args[2];
        for(size_t i = 0; i < len; i++) {
            accum[1] = oldData[i];
            lv_callFunction(&func, 2, accum, &accum[0]);
        }
        res = accum[0];
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

/** Slices the given vect or string */
static TextBufferObj slice(TextBufferObj* args) {

    TextBufferObj res;
    if(args[1].type != OPT_NUMBER || args[2].type != OPT_NUMBER) {
        //check that index args are numbers
        res.type = OPT_UNDEFINED;
    } else if(args[0].type != OPT_VECT && args[0].type != OPT_STRING) {
        //check that the receiver is of appropriate type
        res.type = OPT_UNDEFINED;
    } else {
        int start = (int) args[1].number;
        int end = (int) args[2].number;
        //sanity check
        if(start > end || start < 0 || end < 0) {
            res.type = OPT_UNDEFINED;
        } else if(args[0].type == OPT_VECT) {
            size_t len = args[0].vect->len;
            //bounds check
            if((size_t)start > len || (size_t)end > len) {
                res.type = OPT_UNDEFINED;
            } else {
                //create new vect
                res.type = OPT_VECT;
                res.vect = lv_alloc(sizeof(LvVect) + (end - start) * sizeof(TextBufferObj));
                res.vect->refCount = 0;
                res.vect->len = end - start;
                //copy over elements
                for(size_t i = 0; i < res.vect->len; i++) {
                    res.vect->data[i] = args[0].vect->data[start + i];
                    incRefCount(&res.vect->data[i]);
                }
            }
        } else if(args[0].type == OPT_STRING) {
            size_t len = args[0].str->len;
            //bounds check
            if((size_t)start > len || (size_t)end > len) {
                res.type = OPT_UNDEFINED;
            } else {
                //create new string (include NUL terminator)
                res.type = OPT_STRING;
                res.str = lv_alloc(sizeof(LvString) + (end - start + 1));
                res.str->refCount = 0;
                res.str->len = end - start;
                //copy over elements
                memcpy(res.str->value, &args[0].str->value[start], res.str->len);
                res.str->value[res.str->len] = '\0';
            }
        } else {
            res.type = OPT_UNDEFINED;
        }
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
        op->varargs = false; \
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
    MK_FUNC(cval, 2);
    MK_FUNC(cat, 1); op->varargs = true;
    MK_FUNC(call, 2);
    MK_FUNCN(at, 2);
    MK_FUNCR(bool, 1);
    MK_FUNCN(eq, 2);
    MK_FUNCN(lt, 2);
    MK_FUNCN(ge, 2);
    MK_FUNCN(add, 2);
    MK_FUNCN(sub, 2);
    MK_FUNCN(mul, 2);
    MK_FUNCR(div, 2);
    MK_FUNCN(rem, 2);
    MK_FUNCR(pow, 2);
    MK_FUNCN(pos, 1);
    MK_FUNCN(neg, 1);
    MK_FUNCN(len, 1);
    MK_FUNCR(sin, 1);
    MK_FUNCR(cos, 1);
    MK_FUNCR(tan, 1);
    MK_FUNCR(asin, 1);
    MK_FUNCR(acos, 1);
    MK_FUNCR(atan, 1);
    MK_FUNCR(atan2, 2);
    MK_FUNCR(sinh, 1);
    MK_FUNCR(cosh, 1);
    MK_FUNCR(tanh, 1);
    MK_FUNCR(exp, 1);
    MK_FUNCR(log, 1);
    MK_FUNCR(log10, 1);
    MK_FUNCR(sqrt, 1);
    MK_FUNCR(ceil, 1);
    MK_FUNCR(floor, 1);
    MK_FUNC_IMPL(fabs_, 1, "__abs__");
    MK_FUNCR(round, 1);
    MK_FUNCN(sgn, 1);
    MK_FUNCN(map, 2);
    MK_FUNCN(filter, 2);
    MK_FUNCN(fold, 3);
    MK_FUNCN(slice, 3);
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
