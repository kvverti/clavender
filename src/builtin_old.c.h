#include "builtin.h"
#include "lavender.h"
#include "expression.h"
#include "operator.h"
#include "builtin_funcs.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>

static TextBufferObj indirect(TextBufferObj* args, OpType type, char* key) {

    TextBufferObj res;
    Hashtable* tbl = lv_blt_getFunctionTable(type);
    Builtin b = NULL;
    if(tbl != NULL) {
        b = lv_tbl_get(tbl, key);
    }
    if(b != NULL) {
        res = b(args);
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

static bool equal(TextBufferObj* a, TextBufferObj* b);

bool lv_blt_equal(TextBufferObj* a, TextBufferObj* b) {

    TextBufferObj eq;
    TextBufferObj ab[2] = { *a, *b };
    lv_callFunction(&lv_globalEquals, 2, ab, &eq);
    return eq.type == OPT_INTEGER ? eq.integer : equal(a, b);
}

// evaluates any by-name expressions in the arguments
static void getArgs(TextBufferObj* dst, TextBufferObj* src, size_t len) {

    for(size_t i = 0; i < len; i++) {
        if(!lv_evalByName(&src[i], &dst[i])) {
            dst[i] = src[i];
        }
        if(dst[i].type & LV_DYNAMIC) {
            ++*dst[i].refCount;
        }
    }
}

static void clearArgs(TextBufferObj* args, size_t len) {

    lv_expr_cleanup(args, len);
}

/**
 * Replace by-name expression arguments with their actual results,
 * in place.
 */
static void evalByNameArgs(TextBufferObj* args, size_t len) {

    for(size_t i = 0; i < len; i++) {
        TextBufferObj tmp;
        if(lv_evalByName(&args[i], &tmp)) {
            if(tmp.type & LV_DYNAMIC) {
                ++*tmp.refCount;
            }
            lv_expr_cleanup(&args[i], 1);
            args[i] = tmp;
        }
    }
}

/**
 * Converts a string to a symbol.
 */
static TextBufferObj symb(TextBufferObj* args) {

    TextBufferObj arg, res;
    getArgs(&arg, args, 1);
    if(arg.type == OPT_STRING) {
        res = lv_tb_getSymb(arg.str->value);
    } else {
        res.type = OPT_UNDEFINED;
    }
    clearArgs(&arg, 1);
    return res;
}

/**
 * Returns whether the argument is defined (i.e. not undefined).
 */
static TextBufferObj defined(TextBufferObj* args) {

    TextBufferObj arg, res;
    getArgs(&arg, args, 1);
    res.type = OPT_INTEGER;
    res.integer = (arg.type != OPT_UNDEFINED);
    clearArgs(&arg, 1);
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

#define NUM_TYPES 8
static LvString* types[NUM_TYPES];

static void mkTypes(void) {

    #define INIT(i, n) \
        assert(i < NUM_TYPES); \
        types[i] = lv_alloc(sizeof(LvString) + sizeof(n)); \
        types[i]->len = sizeof(n) - 1; \
        types[i]->refCount = 1; \
        memcpy(types[i]->value, n, sizeof(n))
    INIT(0, "undefined");
    INIT(1, "number");
    INIT(2, "string");
    INIT(3, "vect");
    INIT(4, "function");
    INIT(5, "int");
    INIT(6, "symb");
    INIT(7, "map");
    #undef INIT
}

/**
 * Returns the type of this object, as a string.
 */
static TextBufferObj typeof_(TextBufferObj* args) {

    TextBufferObj arg, res;
    getArgs(&arg, args, 1);
    res.type = OPT_STRING;
    switch(arg.type) {
        case OPT_UNDEFINED:
            res.str = types[0];
            break;
        case OPT_NUMBER:
            res.str = types[1];
            break;
        case OPT_INTEGER:
            res.str = types[5];
            break;
        case OPT_SYMB:
            res.str = types[6];
            break;
        case OPT_STRING:
            res.str = types[2];
            break;
        case OPT_VECT:
            res.str = types[3];
            break;
        case OPT_MAP:
            res.str = types[7];
            break;
        case OPT_CAPTURE:
        case OPT_FUNCTION_VAL:
            res.str = types[4];
            break;
        default:
            assert(false);
    }
    clearArgs(&arg, 1);
    return res;
}

static void incRefCount(TextBufferObj* obj) {

    if(obj->type & LV_DYNAMIC)
        ++*obj->refCount;
}

static inline bool isNegative(uint64_t repr) {

    return repr >> 63;
}

/** Compares two 64bit integers in two's compl representation */
static int intCmp(uint64_t a, uint64_t b) {

    if(a == b)
        return 0;
    bool negA = isNegative(a);
    bool negB = isNegative(b);
    if(negA || negB)
        return a < b ? 1 : -1;
    else
        return a < b ? -1 : 1;
}

/** Converts a Lavender int to a num. */
static double intToNum(uint64_t a) {

    //turn into positive form
    //(INT_MIN is its own positive form)
    bool negA = isNegative(a);
    if(negA)
        a = -a;
    return copysign((double)a, -negA);
}

/**
 * Gets the n'th capture value from the given function.
 */
static TextBufferObj cval(TextBufferObj* _args) {

    TextBufferObj args[2], res;
    getArgs(args, _args, 2);
    if((args[0].type == OPT_CAPTURE)
    && (args[1].type == OPT_INTEGER)
    && (!isNegative(args[1].integer) && args[1].integer < args[0].capfunc->captureCount)) {
        res = args[0].capture->value[(size_t)args[1].integer];
    } else {
        res.type = OPT_UNDEFINED;
    }
    clearArgs(args, 2);
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
        // TextBufferObj* obj = &args[0].vect->data[i];
        TextBufferObj obj;
        getArgs(&obj, &args[0].vect->data[i], 1);
        len += obj.type == OPT_VECT ? obj.vect->len : 1;
        clearArgs(&obj, 1);
        // len += obj->type == OPT_VECT ? obj->vect->len : 1;
    }
    res.vect = lv_alloc(sizeof(LvVect) + len * sizeof(TextBufferObj));
    res.vect->refCount = 0;
    res.vect->len = len;
    size_t idx = 0;
    for(size_t i = 0; i < args[0].vect->len; i++) {
        // TextBufferObj* obj = &args[0].vect->data[i];
        TextBufferObj obj;
        getArgs(&obj, &args[0].vect->data[i], 1);
        if(obj.type == OPT_VECT) {
            for(int j = 0; j < obj.vect->len; j++) {
                incRefCount(&obj.vect->data[j]);
                res.vect->data[idx++] = obj.vect->data[j];
            }
        } else {
            incRefCount(&obj);
            res.vect->data[idx++] = obj;
        }
        clearArgs(&obj, 1);
    }
    assert(idx == len);
    return res;
}

/**
 * Calls the given function with the given vector of
 * arguments. Varargs functions have extra parameters
 * packed into the varargs vector.
 */
static TextBufferObj call(TextBufferObj* _args) {

    TextBufferObj args[2], res;
    getArgs(args, _args, 2);
    if(args[1].type != OPT_VECT) {
        res.type = OPT_UNDEFINED;
    } else {
        lv_callFunction(&args[0], args[1].vect->len, args[1].vect->data, &res);
    }
    clearArgs(args, 2);
    return res;
}

/**
 * Returns the i'th element of the given string or vect.
 */
static TextBufferObj at(TextBufferObj* _args) {

    TextBufferObj args[2], res;
    getArgs(args, _args, 2);
    res = indirect(args, args[1].type, ".at");
    if(res.type != OPT_UNDEFINED) {
        ;
    } else {
        res.type = OPT_UNDEFINED;
    }
    clearArgs(args, 2);
    return res;
}

bool lv_blt_toBool(TextBufferObj* obj) {

    switch(obj->type) {
        case OPT_UNDEFINED: return false;
        case OPT_NUMBER: return obj->number != 0.0;
        case OPT_INTEGER: return obj->integer != 0;
        case OPT_STRING: return obj->str->len != 0;
        case OPT_VECT: return obj->vect->len != 0;
        case OPT_MAP: return obj->map->len != 0;
        default: return true;
    }
}

/**
 * Concatenates two values together.
 */
static TextBufferObj concat(TextBufferObj* args) {

    TextBufferObj res;
    evalByNameArgs(args, 2);
    if(args[0].type != args[1].type) {
        res.type = OPT_UNDEFINED;
    } else {
        res = indirect(args, args[0].type, ".cat");
        if(res.type != OPT_UNDEFINED) {
            ;
        } else {
            switch(args[0].type) {
                default:
                    res.type = OPT_UNDEFINED;
            }
        }
    }
    return res;
}

/**
 * Converts to bool.
 */
static TextBufferObj bool_(TextBufferObj* _args) {

    TextBufferObj args[1], res;
    getArgs(args, _args, 1);
    res.type = OPT_INTEGER;
    res.integer = lv_blt_toBool(&args[0]);
    clearArgs(args, 1);
    return res;
}

/**
 * Converts object to string.
 */
static TextBufferObj str(TextBufferObj* _args) {

    if(_args[0].type == OPT_STRING)
        return _args[0];
    TextBufferObj args[1], res;
    getArgs(args, _args, 1);
    res = indirect(args, args[0].type, ".str");
    if(res.type != OPT_UNDEFINED) {
        ;
    } else {
        res.type = OPT_STRING;
        res.str = lv_tb_getString(&args[0]);
    }
    clearArgs(args, 1);
    return res;
}

/** Converts to int. */
static TextBufferObj int_(TextBufferObj* _args) {

    TextBufferObj args[1], res;
    getArgs(args, _args, 1);
    res = indirect(args, args[0].type, ".int");
    if(res.type != OPT_UNDEFINED) {
        ;
    } else if(args[0].type == OPT_INTEGER)
        return args[0];
    else if(args[0].type == OPT_NUMBER) {
        //get magnitude
        double mag = args[0].number;
        if(!isfinite(mag)) {
            res.type = OPT_UNDEFINED;
        } else {
            res.type = OPT_INTEGER;
            res.integer = (uint64_t) mag;
        }
    } else {
        res.type = OPT_UNDEFINED;
    }
    clearArgs(args, 1);
    return res;
}

/** Converts object to number. */
static TextBufferObj num(TextBufferObj* _args) {

    TextBufferObj args[1], res;
    getArgs(args, _args, 1);
    res = indirect(args, args[0].type, ".num");
    if(res.type != OPT_UNDEFINED) {
        ;
    } else if(args[0].type == OPT_NUMBER)
        return args[0];
    else if(args[0].type == OPT_INTEGER) {
        res.type = OPT_NUMBER;
        res.number = intToNum(args[0].integer);
    } else {
        res.type = OPT_UNDEFINED;
    }
    clearArgs(args, 1);
    return res;
}

/**
 * Returns length of object if defined.
 */
static TextBufferObj len(TextBufferObj* _args) {

    TextBufferObj args[1], res;
    getArgs(args, _args, 1);
    res = indirect(args, args[0].type, ".len");
    if(res.type != OPT_UNDEFINED) {
        ;
    } else
    switch(args[0].type) {
        case OPT_FUNCTION_VAL:
            //arity of function
            res.type = OPT_INTEGER;
            res.integer = args[0].func->arity;
            break;
        case OPT_CAPTURE:
            res.type = OPT_INTEGER;
            res.integer = args[0].capfunc->arity - args[0].capfunc->captureCount;
            break;
        case OPT_VECT:
            res.type = OPT_INTEGER;
            res.integer = args[0].vect->len;
            break;
        case OPT_MAP:
            res.type = OPT_INTEGER;
            res.integer = args[0].map->len;
            break;
        default:
            res.type = OPT_UNDEFINED;
    }
    clearArgs(args, 1);
    return res;
}

static uint64_t hashOf(void* mem, size_t len) {
    //djb2 hash
    uint64_t res = 5381;
    unsigned char* str = mem;
    while(len-- > 0)
        res = ((res << 5) + res) + *str++;
    return res;
}

static uint64_t hashcode(TextBufferObj* arg) {

    uint64_t res;
    switch(arg->type) {
        case OPT_UNDEFINED:
            res = 0;
            break;
        case OPT_NUMBER:
            res = hashOf(&arg->number, sizeof(arg->number));
            break;
        case OPT_INTEGER:
            res = arg->integer;
            break;
        case OPT_SYMB:
            res = arg->symbIdx;
            break;
        case OPT_STRING:
            res = hashOf(arg->str->value, arg->str->len);
            break;
        case OPT_FUNCTION_VAL:
            res = (uint64_t)arg->func;
            break;
        case OPT_CAPTURE: {
            uint64_t h = 5381;
            for(size_t i = 0; i < arg->capfunc->captureCount; i++) {
                h = ((h << 5) + h) + lv_blt_hash(&arg->capture->value[i]);
            }
            res = h;
            break;
        }
        case OPT_VECT: {
            uint64_t h = 5381;
            for(size_t i = 0; i < arg->vect->len; i++) {
                h = ((h << 5) + h) + lv_blt_hash(&arg->vect->data[i]);
            }
            res = h;
            break;
        }
        case OPT_MAP: {
            uint64_t h = 5381;
            for(size_t i = 0; i < arg->map->len; i++) {
                h = ((h << 5) + h) + arg->map->data[i].hash;
                h = ((h << 5) + h) + lv_blt_hash(&arg->map->data[i].value);
            }
            res = h;
            break;
        }
        default:
            assert(false);
    }
    return res;
}

static TextBufferObj hash(TextBufferObj* _args) {

    TextBufferObj arg, res;
    getArgs(&arg, _args, 1);
    res.type = OPT_INTEGER;
    res.integer = hashcode(&arg);
    clearArgs(&arg, 1);
    return res;
}

uint64_t lv_blt_hash(TextBufferObj* a) {

    TextBufferObj res;
    lv_callFunction(&lv_globalHash, 1, a, &res);
    return res.type == OPT_INTEGER ? res.integer : hashcode(a);
}

static bool equal(TextBufferObj* a, TextBufferObj* b) {

    if(a->type != b->type) {
        //can't be equal if they have different types
        //numbers and integers are not equal!
        return false;
    }
    switch(a->type) {
        case OPT_UNDEFINED:
            return true;
        case OPT_NUMBER:
            return a->number == b->number;
        case OPT_INTEGER:
            return a->integer == b->integer;
        case OPT_SYMB:
            return a->symbIdx == b->symbIdx;
        case OPT_FUNCTION_VAL:
            return a->func == b->func;
        case OPT_CAPTURE:
            if(a->capfunc != b->capfunc)
                return false;
            for(int i = 0; i < a->capfunc->captureCount; i++) {
                if(!lv_blt_equal(&a->capture->value[i], &b->capture->value[i]))
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
static TextBufferObj eq(TextBufferObj* _args) {

    TextBufferObj args[2], res;
    getArgs(args, _args, 2);
    res = indirect(args, args[0].type, ".eq");
    if(res.type != OPT_UNDEFINED) {
        ;
    } else {
        res.type = OPT_INTEGER;
        res.integer = equal(&args[0], &args[1]);
    }
    clearArgs(args, 2);
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
        case OPT_INTEGER:
            return intCmp(a->integer, b->integer) < 0;
            break;
        case OPT_SYMB:
            return (a->symbIdx < b->symbIdx);
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
                    if(lv_blt_lt(&a->capture->value[i], &b->capture->value[i])) {
                        return true;
                    } else if(lv_blt_lt(&b->capture->value[i], &a->capture->value[i])) {
                        return false;
                    }
                }
                return false;
            }
            return (uintptr_t)a->capfunc < (uintptr_t)b->capfunc;
        default:
            assert(false);
    }
}

bool lv_blt_lt(TextBufferObj* a, TextBufferObj* b) {

    TextBufferObj lt;
    TextBufferObj ab[2] = { *a, *b };
    lv_callFunction(&lv_globalLt, 2, ab, &lt);
    return lt.type == OPT_INTEGER ? lt.integer : ltImpl(a, b);
}

static TextBufferObj lt(TextBufferObj* _args) {

    TextBufferObj args[2], res;
    getArgs(args, _args, 2);
    res.type = OPT_INTEGER;
    if((args[0].type == OPT_NUMBER && args[1].type == OPT_NUMBER)
    && ((args[0].number != args[0].number) || (args[1].number != args[1].number))) {
        //one of them is NaN
        res.number = 0.0;
        return res;
    }
    res = indirect(args, args[0].type, ".lt");
    if(res.type != OPT_UNDEFINED) {
        ;
    } else {
        res.type = OPT_INTEGER;
        res.integer = ltImpl(&args[0], &args[1]);
    }
    clearArgs(args, 2);
    return res;
}

//we need both lt and ge because NaN always compares false.
//the Lavender comparison functions use either lt or ge
//as appropriate.
static TextBufferObj ge(TextBufferObj* _args) {

    TextBufferObj args[2], res;
    getArgs(args, _args, 2);
    res.type = OPT_INTEGER;
    if((args[0].type == OPT_NUMBER && args[1].type == OPT_NUMBER)
    && ((args[0].number != args[0].number) || (args[1].number != args[1].number))) {
        //one of them is NaN
        res.number = 0.0;
        return res;
    }
    res = indirect(args, args[0].type, ".lt");
    if(res.type != OPT_UNDEFINED) {
        res.integer = !res.integer;
    } else {
        res.type = OPT_INTEGER;
        res.integer = !ltImpl(&args[0], &args[1]);
    }
    clearArgs(args, 2);
    return res;
}

typedef enum NumResult { NR_ERROR, NR_NUMBER, NR_INTEGER } NumResult;
typedef union NumType { uint64_t integer; double number; } NumType;

/**
 * Parses the two arguments into numbers if possible, widening from
 * integer to number if necessary.
 */
static NumResult getObjsAsNumbers(TextBufferObj args[2], NumType res[2]) {
    switch(args[0].type) {
        case OPT_INTEGER:
            switch(args[1].type) {
                case OPT_INTEGER:
                    //both are integers
                    res[0].integer = args[0].integer;
                    res[1].integer = args[1].integer;
                    return NR_INTEGER;
                case OPT_NUMBER:
                    //convert args[0] to number
                    res[0].number = intToNum(args[0].integer);
                    res[1].number = args[1].number;
                    return NR_NUMBER;
                default:
                    return NR_ERROR;
            }
        case OPT_NUMBER:
            switch(args[1].type) {
                case OPT_INTEGER:
                    //convert args[1] to number
                    res[0].number = args[0].number;
                    res[1].number = intToNum(args[1].integer);
                    return NR_NUMBER;
                case OPT_NUMBER:
                    //both are numbers
                    res[0].number = args[0].number;
                    res[1].number = args[1].number;
                    return NR_NUMBER;
                default:
                    return NR_ERROR;
            }
        default:
            return NR_ERROR;
    }
}

/**
 * Addition and string concatenation. Assumes two arguments.
 */
static TextBufferObj add(TextBufferObj* _args) {

    TextBufferObj args[2], res;
    NumType nums[2];
    getArgs(args, _args, 2);
    switch(getObjsAsNumbers(args, nums)) {
        case NR_NUMBER:
            res.type = OPT_NUMBER;
            res.number = nums[0].number + nums[1].number;
            break;
        case NR_INTEGER:
            res.type = OPT_INTEGER;
            res.integer = nums[0].integer + nums[1].integer;
            break;
        case NR_ERROR:
            res.type = OPT_UNDEFINED;
            break;
    }
    clearArgs(args, 2);
    return res;
}

/**
 * Subtraction. Assumes two arguments.
 */
static TextBufferObj sub(TextBufferObj* _args) {

    TextBufferObj args[2], res;
    NumType nums[2];
    getArgs(args, _args, 2);
    switch(getObjsAsNumbers(args, nums)) {
        case NR_NUMBER:
            res.type = OPT_NUMBER;
            res.number = nums[0].number - nums[1].number;
            break;
        case NR_INTEGER:
            res.type = OPT_INTEGER;
            res.integer = nums[0].integer - nums[1].integer;
            break;
        case NR_ERROR:
            res.type = OPT_UNDEFINED;
            break;
    }
    clearArgs(args, 2);
    return res;
}

/** Multiplication */
static TextBufferObj mul(TextBufferObj* _args) {

    TextBufferObj args[2], res;
    NumType nums[2];
    getArgs(args, _args, 2);
    switch(getObjsAsNumbers(args, nums)) {
        case NR_NUMBER:
            res.type = OPT_NUMBER;
            res.number = nums[0].number * nums[1].number;
            break;
        case NR_INTEGER:
            res.type = OPT_INTEGER;
            res.integer = nums[0].integer * nums[1].integer;
            break;
        case NR_ERROR:
            res.type = OPT_UNDEFINED;
            break;
    }
    clearArgs(args, 2);
    return res;
}

static uint64_t intDiv(uint64_t a, uint64_t b, bool rem) {

    //twos complement division
    assert(b != 0);
    bool quotNegative;
    {
        //turn into positive forms
        //(INT_MIN is its own positive form)
        bool negA = isNegative(a);
        bool negB = isNegative(b);
        if(negA)
            a = -a;
        if(negB)
            b = -b;
        //remainders only depend on the dividend
        quotNegative = negA ^ (negB && !rem);
    }
    uint64_t quot = rem ? (a % b) : (a / b);
    return quotNegative ? -quot : quot;
}

static double numDiv(double a, double b, bool rem) {

    if(b == 0.0) {
        double sign = copysign(1.0, b) * copysign(1.0, a);
        double ans = (a == 0.0 ? NAN : INFINITY);
        ans = copysign(ans, sign);
        return ans;
    } else {
        return rem ? fmod(a, b) : (a / b);
    }
}

/** Division */
static TextBufferObj div_(TextBufferObj* _args) {

    TextBufferObj args[2], res;
    NumType nums[2];
    getArgs(args, _args, 2);
    switch(getObjsAsNumbers(args, nums)) {
        case NR_INTEGER:
            nums[0].number = intToNum(nums[0].integer);
            nums[1].number = intToNum(nums[1].integer);
            //fallthrough
        case NR_NUMBER:
            res.type = OPT_NUMBER;
            res.number = numDiv(nums[0].number, nums[1].number, false);
            break;
        case NR_ERROR:
            res.type = OPT_UNDEFINED;
            break;
    }
    clearArgs(args, 2);
    return res;
}

/** Integer division */
static TextBufferObj idiv(TextBufferObj* _args) {

    TextBufferObj args[2], res;
    NumType nums[2];
    getArgs(args, _args, 2);
    switch(getObjsAsNumbers(args, nums)) {
        case NR_NUMBER: {
            double a = nums[0].number;
            double b = nums[1].number;
            if(isfinite(a) && isfinite(b) && b != 0.0) {
                res.type = OPT_INTEGER;
                res.integer = (uint64_t)((a - numDiv(a, b, true)) / b);
            } else {
                res.type = OPT_UNDEFINED;
            }
            break;
        }
        case NR_INTEGER: {
            uint64_t a = nums[0].integer;
            uint64_t b = nums[1].integer;
            if(b != 0) {
                res.type = OPT_INTEGER;
                res.integer = intDiv(a, b, false);
            } else {
                res.type = OPT_UNDEFINED;
            }
            break;
        }
        case NR_ERROR:
            res.type = OPT_UNDEFINED;
            break;
    }
    clearArgs(args, 2);
    return res;
}

/** Remainder */
static TextBufferObj rem(TextBufferObj* _args) {

    TextBufferObj args[2], res;
    NumType nums[2];
    getArgs(args, _args, 2);
    switch(getObjsAsNumbers(args, nums)) {
        case NR_NUMBER:
            res.type = OPT_NUMBER;
            res.number = numDiv(nums[0].number, nums[1].number, true);
            break;
        case NR_INTEGER:
            res.type = OPT_INTEGER;
            res.integer = intDiv(nums[0].integer, nums[1].integer, true);
            break;
        case NR_ERROR:
            res.type = OPT_UNDEFINED;
            break;
    }
    clearArgs(args, 2);
    return res;
}

/** Exponentiation */
static TextBufferObj pow_(TextBufferObj* _args) {

    TextBufferObj args[2], res;
    NumType nums[2];
    getArgs(args, _args, 2);
    switch(getObjsAsNumbers(args, nums)) {
        case NR_NUMBER:
            res.type = OPT_NUMBER;
            res.number = pow(nums[0].number, nums[1].number);
            break;
        case NR_INTEGER: {
            uint64_t a = nums[0].integer;
            uint64_t b = nums[1].integer;
            if(isNegative(b)) {
                if(a == 0) {
                    res.type = OPT_UNDEFINED;
                } else {
                    //negative powers are equiv. to 1 / (a ** b)
                    res.type = OPT_NUMBER;
                    res.number = pow(intToNum(a), intToNum(b));
                }
            } else if(a == 2) {
                //2 ** x can be implemented with a bit shift by (b & 63)
                res.type = OPT_INTEGER;
                res.integer = UINT64_C(1) << (b & 63);
            } else {
                //algorithm taken from everyone's favorite source of knowledge,
                //stack overflow
                uint64_t powres = 1;
                while(b != 0) {
                    if(b & 1)
                        powres *= a;
                    a *= a;
                    b >>= 1;
                }
                res.type = OPT_INTEGER;
                res.integer = powres;
            }
            break;
        }
        case NR_ERROR:
            res.type = OPT_UNDEFINED;
    }
    clearArgs(args, 2);
    return res;
}

/** Unary + function */
static TextBufferObj pos(TextBufferObj* _args) {

    TextBufferObj args[1], res;
    getArgs(args, _args, 1);
    if(args[0].type == OPT_NUMBER || args[0].type == OPT_INTEGER) {
        return args[0];
    } else {
        res.type = OPT_UNDEFINED;
    }
    clearArgs(args, 1);
    return res;
}

/** Negation function */
static TextBufferObj neg(TextBufferObj* _args) {

    TextBufferObj args[1], res;
    getArgs(args, _args, 1);
    if(args[0].type == OPT_NUMBER) {
        res.type = OPT_NUMBER;
        res.number = -args[0].number;
    } else if(args[0].type == OPT_INTEGER) {
        res.type = OPT_INTEGER;
        res.integer = -args[0].integer;
    } else {
        res.type = OPT_UNDEFINED;
    }
    clearArgs(args, 1);
    return res;
}

#define DECL_MATH_FUNC(fnc) \
static TextBufferObj fnc##_(TextBufferObj* _args) { \
    TextBufferObj args[1], res; \
    getArgs(args, _args, 1); \
    if(args[0].type == OPT_NUMBER) { \
        res.type = OPT_NUMBER; \
        res.number = fnc(args[0].number); \
    } else if(args[0].type == OPT_INTEGER) { \
        res.type = OPT_NUMBER; \
        res.number = fnc(intToNum(args[0].integer)); \
    } else { \
        res.type = OPT_UNDEFINED; \
    } \
    clearArgs(args, 1); \
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
DECL_MATH_FUNC(round);

#undef DECL_MATH_FUNC

static TextBufferObj abs_(TextBufferObj* _args) {

    TextBufferObj args[1], res;
    getArgs(args, _args, 1);
    if(args[0].type == OPT_NUMBER) {
        res.type = OPT_NUMBER;
        res.number = fabs(args[0].number);
    } else if(args[0].type == OPT_INTEGER) {
        uint64_t a = args[0].integer;
        res.type = OPT_INTEGER;
        res.integer = isNegative(a) ? -a : a;
    } else {
        res.type = OPT_UNDEFINED;
    }
    clearArgs(args, 1);
    return res;
}

static TextBufferObj atan2_(TextBufferObj* _args) {

    TextBufferObj args[2], res;
    NumType nums[2];
    getArgs(args, _args, 2);
    switch(getObjsAsNumbers(args, nums)) {
        case NR_INTEGER:
            nums[0].number = intToNum(nums[0].integer);
            nums[1].number = intToNum(nums[1].integer);
            //fallthrough
        case NR_NUMBER:
            res.type = OPT_NUMBER;
            res.number = atan2(nums[0].number, nums[1].number);
            break;
        case NR_ERROR:
            res.type = OPT_UNDEFINED;
            break;
    }
    clearArgs(args, 2);
    return res;
}

/** Sign function. Returns +1 for +0 and -1 for -0. */
static TextBufferObj sgn(TextBufferObj* _args) {

    TextBufferObj args[1], res;
    getArgs(args, _args, 1);
    if(args[0].type == OPT_NUMBER) {
        res.type = OPT_INTEGER;
        // (-inf, -0] -> -1 : [+0, inf) -> +1
        res.integer = 1 - ((bool)signbit(args[0].number) << 1);
    } else if(args[0].type == OPT_INTEGER) {
        uint64_t a = args[0].integer;
        res.type = OPT_INTEGER;
        //[min, -1] -> -1, [0] -> 0, [1, max] -> +1
        res.integer = (a && (a < (UINT64_C(1) << 63))) - (a >> 63);
    } else {
        res.type = OPT_UNDEFINED;
    }
    clearArgs(args, 1);
    return res;
}

//functional functions

/** Functional map */
static TextBufferObj map(TextBufferObj* _args) {

    TextBufferObj args[2], res;
    getArgs(args, _args, 2);
    res = indirect(args, args[0].type, ".map");
    clearArgs(args, 2);
    return res;
}

/** Functional filter */
static TextBufferObj filter(TextBufferObj* _args) {

    TextBufferObj args[2], res;
    getArgs(args, _args, 2);
    res = indirect(args, args[0].type, ".filter");
    if(res.type != OPT_UNDEFINED) {
        ;
    } else {
        res.type = OPT_UNDEFINED;
    }
    clearArgs(args, 2);
    return res;
}

/** Functional fold */
static TextBufferObj fold(TextBufferObj* _args) {

    TextBufferObj args[3], res;
    getArgs(args, _args, 3);
    res = indirect(args, args[0].type, ".fold");
    if(res.type != OPT_UNDEFINED) {
        ;
    } else {
        res.type = OPT_UNDEFINED;
    }
    clearArgs(args, 3);
    return res;
}

/** Slices the given vect or string */
static TextBufferObj slice(TextBufferObj* _args) {

    TextBufferObj args[3], res;
    getArgs(args, _args, 3);
    res = indirect(args, args[0].type, ".slice");
    if(res.type != OPT_UNDEFINED) {
        ;
    } else if(args[1].type != OPT_INTEGER || args[2].type != OPT_INTEGER) {
        //check that index args are numbers
        res.type = OPT_UNDEFINED;
    } else if(args[0].type != OPT_VECT && args[0].type != OPT_STRING) {
        //check that the receiver is of appropriate type
        res.type = OPT_UNDEFINED;
    } else {
        uint64_t start = args[1].integer;
        uint64_t end = args[2].integer;
        //sanity check
        if(start > end || isNegative(start) || isNegative(end)) {
            res.type = OPT_UNDEFINED;
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
    clearArgs(args, 3);
    return res;
}

/** Takes elements from the vect while the predicate is satisfied */
TextBufferObj take(TextBufferObj* _args) {

    TextBufferObj args[2], res;
    getArgs(args, _args, 2);
    res = indirect(args, args[0].type, ".take");
    if(res.type != OPT_UNDEFINED) {
        ;
    } else {
        res.type = OPT_UNDEFINED;
    }
    clearArgs(args, 2);
    return res;
}

/** Drops elements from the vect while the predicate is satisifed */
TextBufferObj skip(TextBufferObj* _args) {

    TextBufferObj args[2], res;
    getArgs(args, _args, 2);
    res = indirect(args, args[0].type, ".skip");
    if(res.type != OPT_UNDEFINED) {
        ;
    } else {
        res.type = OPT_UNDEFINED;
    }
    clearArgs(args, 2);
    return res;
}
