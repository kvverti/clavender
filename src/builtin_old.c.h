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

static TextBufferObj eq(TextBufferObj* args);

bool lv_blt_equal(TextBufferObj* a, TextBufferObj* b) {

    TextBufferObj ab[2] = { *a, *b };
    TextBufferObj eql = eq(ab);
    bool res = lv_blt_toBool(&eql);
    lv_expr_cleanup(&eql, 1);
    return res;
}

static TextBufferObj lookup(TextBufferObj* args) {
    TextBufferObj res;
    getActualArgs(args, 3);
    if(args[0].type != OPT_FUNCTION_VAL) {
        res.type = OPT_UNDEFINED;
        return res;
    }
    FuncNamespace fns = args[0].func->fixing == FIX_PRE ? FNS_PREFIX : FNS_INFIX;
    char* nm = strchr(args[0].func->name, ':') + 1;
    char* ns;
    char* colpos;
    char* fnm;
    // get the namespace
    switch(args[1].type) {
        case OPT_FUNCTION_VAL:
            fnm = args[1].func->name;
            colpos = strchr(fnm, ':');
            ns = lv_alloc(colpos - fnm + 1);
            strncpy(ns, fnm, colpos - fnm);
            ns[colpos - fnm] = '\0';
            break;
        case OPT_CAPTURE:
            fnm = args[1].capfunc->name;
            colpos = strchr(fnm, ':');
            ns = lv_alloc(colpos - fnm + 1);
            strncpy(ns, fnm, colpos - fnm);
            ns[colpos - fnm] = '\0';
            break;
        case OPT_MAP:
            ns = "map";
            break;
        case OPT_VECT:
            ns = "vect";
            break;
        case OPT_SYMB:
            ns = "symb";
            break;
        case OPT_STRING:
            ns = "str";
            break;
        case OPT_NUMBER:
            ns = "num";
            break;
        case OPT_INTEGER:
        case OPT_BIGINT:
            ns = "int";
            break;
        case OPT_UNDEFINED:
            ns = "undefined";
            break;
        default:
            assert(((void)"invalid namespace", false));
            res.type = OPT_UNDEFINED;
    }
    Operator* op = lv_op_getScopedOperator(ns, nm, fns);
    if(op == NULL) {
        res = args[2];
    } else {
        res.type = OPT_FUNCTION_VAL;
        res.func = op;
    }
    if(args[1].type == OPT_CAPTURE || args[1].type == OPT_FUNCTION_VAL) {
        lv_free(ns);
    }
    return res;
}

/**
 * Converts a string to a symbol.
 */
static TextBufferObj symb(TextBufferObj* args) {

    TextBufferObj res;
    getActualArgs(args, 1);
    if(args[0].type == OPT_STRING) {
        res = lv_tb_getSymb(args[0].str->value);
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

/**
 * Returns whether the argument is defined (i.e. not undefined).
 */
static TextBufferObj defined(TextBufferObj* args) {

    TextBufferObj res;
    getActualArgs(args, 1);
    res.type = OPT_INTEGER;
    res.integer = (args[0].type != OPT_UNDEFINED);
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
static size_t types[NUM_TYPES];

static void mkTypes(void) {

    #define INIT(i, n) \
        assert(i < NUM_TYPES); \
        types[i] = lv_tb_getSymb(n).integer;
    INIT(0, "undefined");
    INIT(1, "num");
    INIT(2, "str");
    INIT(3, "vect");
    INIT(4, "func");
    INIT(5, "int");
    INIT(6, "symb");
    INIT(7, "map");
    #undef INIT
}

/**
 * Returns the type of this object, as a string.
 */
static TextBufferObj typeof_(TextBufferObj* args) {

    TextBufferObj res;
    getActualArgs(args, 1);
    res.type = OPT_SYMB;
    switch(args[0].type) {
        case OPT_UNDEFINED:
            res.symbIdx = types[0];
            break;
        case OPT_NUMBER:
            res.symbIdx = types[1];
            break;
        case OPT_INTEGER:
        case OPT_BIGINT:
            res.symbIdx = types[5];
            break;
        case OPT_SYMB:
            res.symbIdx = types[6];
            break;
        case OPT_STRING:
            res.symbIdx = types[2];
            break;
        case OPT_VECT:
            res.symbIdx = types[3];
            break;
        case OPT_MAP:
            res.symbIdx = types[7];
            break;
        case OPT_CAPTURE:
        case OPT_FUNCTION_VAL:
            res.symbIdx = types[4];
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

static inline bool isNegative(uint64_t repr) {

    return repr >> 63;
}

/**
 * Gets the n'th capture value from the given function.
 */
static TextBufferObj cval(TextBufferObj* args) {

    TextBufferObj res;
    getActualArgs(args, 2);
    if((args[0].type == OPT_CAPTURE)
    && (args[1].type == OPT_INTEGER)
    && (!isNegative(args[1].integer) && args[1].integer < args[0].capfunc->captureCount)) {
        res = args[0].capture->value[(size_t)args[1].integer];
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
    getActualArgs(args[0].vect->data, args[0].vect->len);
    for(size_t i = 0; i < args[0].vect->len; i++) {
        TextBufferObj* obj = &args[0].vect->data[i];
        // getActualArgs(obj, 1);
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
    getActualArgs(args, 2);
    if(args[1].type != OPT_VECT) {
        res.type = OPT_UNDEFINED;
    } else {
        lv_callFunction(&args[0], args[1].vect->len, args[1].vect->data, &res);
    }
    return res;
}

/**
 * Returns the i'th element of the given string or vect.
 */
static TextBufferObj at(TextBufferObj* args) {

    getActualArgs(args, 2);
    return indirect(args, args[1].type, ".at");
}

bool lv_blt_toBool(TextBufferObj* obj) {

    switch(obj->type) {
        case OPT_UNDEFINED: return false;
        case OPT_NUMBER: return obj->number != 0.0;
        case OPT_INTEGER: return obj->integer != 0;
        case OPT_BIGINT: return true;
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

    getActualArgs(args, 2);
    return indirect(args, args[0].type, ".cat");
}

/**
 * Converts to bool.
 */
static TextBufferObj bool_(TextBufferObj* args) {

    TextBufferObj res;
    getActualArgs(args, 1);
    res.type = OPT_INTEGER;
    res.integer = lv_blt_toBool(&args[0]);
    return res;
}

static LvString* defaultStr(TextBufferObj* obj) {

    LvString* res;
    switch(obj->type) {
        case OPT_UNDEFINED: {
            static char str[] = "<undefined>";
            res = lv_alloc(sizeof(LvString) + sizeof(str));
            res->refCount = 0;
            res->len = sizeof(str) - 1;
            strcpy(res->value, str);
            return res;
        }
        case OPT_SYMB: {
            char* val = lv_tb_getSymbName(obj->symbIdx);
            size_t len = strlen(val);
            res = lv_alloc(sizeof(LvString) + len + 2);
            res->refCount = 0;
            res->len = len + 1;
            res->value[0] = '.';
            memcpy(res->value + 1, val, len + 1);
            return res;
        }
        case OPT_TAIL:
        case OPT_FUNCTION:
        case OPT_FUNCTION_VAL: {
            size_t len = strlen(obj->func->name);
            res = lv_alloc(sizeof(LvString) + len + 1);
            res->refCount = 0;
            res->len = len;
            strcpy(res->value, obj->func->name);
            return res;
        }
        default: {
            static char str[] = "<internal operator>";
            res = lv_alloc(sizeof(LvString) + sizeof(str));
            res->refCount = 0;
            res->len = sizeof(str) - 1;
            strcpy(res->value, str);
            return res;
        }
    }
}

/**
 * Converts object to string.
 */
static TextBufferObj str(TextBufferObj* args) {

    getActualArgs(args, 1);
    return (TextBufferObj) {
        .type = OPT_STRING,
        .str = lv_blt_str(args)
    };
}

LvString* lv_blt_str(TextBufferObj* obj) {

    TextBufferObj res = indirect(obj, obj->type, ".str");
    if(res.type == OPT_UNDEFINED) {
        return defaultStr(obj);
    }
    return res.str;
}

/** Converts to int. */
static TextBufferObj int_(TextBufferObj* args) {

    getActualArgs(args, 1);
    return indirect(args, args[0].type, ".int");
}

/** Converts object to number. */
static TextBufferObj num(TextBufferObj* args) {

    // TextBufferObj res;
    getActualArgs(args, 1);
    return indirect(args, args[0].type, ".num");
}

/**
 * Returns length of object if defined.
 */
static TextBufferObj len(TextBufferObj* args) {

    getActualArgs(args, 1);
    return indirect(args, args[0].type, ".len");
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
        case OPT_SYMB:
            res = arg->symbIdx;
            break;
        case OPT_STRING:
            res = hashOf(arg->str->value, arg->str->len);
            break;
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

static TextBufferObj hash(TextBufferObj* args) {

    TextBufferObj res;
    getActualArgs(args, 1);
    res = indirect(args, args[0].type, ".hash");
    if(res.type == OPT_UNDEFINED) {
        res.type = OPT_INTEGER;
        res.integer = hashcode(&args[0]);
    }
    return res;
}

uint64_t lv_blt_hash(TextBufferObj* a) {

    TextBufferObj hashcd = hash(a);
    uint64_t res = hashcd.integer;
    lv_expr_cleanup(&hashcd, 1);
    return res;
}

static bool equal(TextBufferObj* a, TextBufferObj* b) {
    assert(a->type == b->type);
    switch(a->type) {
        case OPT_UNDEFINED:
            return true;
        case OPT_SYMB:
            return a->symbIdx == b->symbIdx;
        default:
            assert(false);
    }
}

/**
 * Compares two objects for equality.
 */
static TextBufferObj eq(TextBufferObj* args) {

    TextBufferObj res;
    getActualArgs(args, 2);
    if(args[0].type != args[1].type) {
        // different types cannot be equal
        res.type = OPT_INTEGER;
        res.integer = 0;
    } else {
        res = indirect(args, args[0].type, ".eq");
        if(res.type == OPT_UNDEFINED) {
            res.type = OPT_INTEGER;
            res.integer = equal(&args[0], &args[1]);
        }
    }
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
        case OPT_SYMB:
            return (a->symbIdx < b->symbIdx);
        default:
            assert(false);
    }
}

static TextBufferObj lt(TextBufferObj* args) {

    TextBufferObj res;
    getActualArgs(args, 2);
    OpType atyp = args[0].type, btyp = args[1].type;
    if((atyp == btyp) ||
        (atyp == OPT_INTEGER && btyp == OPT_BIGINT) ||
        (atyp == OPT_BIGINT && btyp == OPT_INTEGER)) {
        res = indirect(args, args[0].type, ".lt");
        if(res.type == OPT_UNDEFINED) {
            res.type = OPT_INTEGER;
            res.integer = ltImpl(&args[0], &args[1]);
        }
    } else {
        res.type = OPT_INTEGER;
        res.integer = args[0].type < args[1].type;
    }
    return res;
}

bool lv_blt_lt(TextBufferObj* a, TextBufferObj* b) {

    TextBufferObj ab[2] = { *a, *b };
    TextBufferObj ltv = lt(ab);
    bool res = lv_blt_toBool(&ltv);
    lv_expr_cleanup(&ltv, 1);
    return res;
}

//we need both lt and ge because NaN always compares false.
//the Lavender comparison functions use either lt or ge
//as appropriate.
static TextBufferObj ge(TextBufferObj* args) {

    TextBufferObj res;
    getActualArgs(args, 2);
    if((args[0].type == OPT_NUMBER && args[1].type == OPT_NUMBER)
    && (isnan(args[0].number) || isnan(args[1].number))) {
        //one of them is NaN
        res.type = OPT_INTEGER;
        res.integer = 0;
    } else {
        res = lt(args);
        res.integer = !res.integer;
    }
    return res;
}

/**
 * Addition and string concatenation. Assumes two arguments.
 */
static TextBufferObj add(TextBufferObj* args) {

    getActualArgs(args, 2);
    return indirect(args, args[0].type, ".add");
}

/**
 * Subtraction. Assumes two arguments.
 */
static TextBufferObj sub(TextBufferObj* args) {

    getActualArgs(args, 2);
    return indirect(args, args[0].type, ".sub");
}

/** Multiplication */
static TextBufferObj mul(TextBufferObj* args) {

    getActualArgs(args, 2);
    return indirect(args, args[0].type, ".mul");
}

/** Division */
static TextBufferObj div_(TextBufferObj* args) {

    getActualArgs(args, 2);
    return indirect(args, args[0].type, ".rdiv");
}

/** Integer division */
static TextBufferObj idiv(TextBufferObj* args) {

    getActualArgs(args, 2);
    return indirect(args, args[0].type, ".div");
}

/** Remainder */
static TextBufferObj rem(TextBufferObj* args) {

    getActualArgs(args, 2);
    return indirect(args, args[0].type, ".rem");
}

/** Exponentiation */
static TextBufferObj pow_(TextBufferObj* args) {

    getActualArgs(args, 2);
    return lv_int_pow(args);
}

/** Unary + function */
static TextBufferObj pos(TextBufferObj* args) {

    getActualArgs(args, 1);
    return indirect(args, args[0].type, ".pos");
}

/** Negation function */
static TextBufferObj neg(TextBufferObj* args) {

    getActualArgs(args, 1);
    return indirect(args, args[0].type, ".neg");
}

#define DECL_MATH_FUNC(fnc) \
static TextBufferObj fnc##_(TextBufferObj* args) { \
    TextBufferObj res; \
    getActualArgs(args, 1); \
    if(args[0].type == OPT_NUMBER) { \
        res.type = OPT_NUMBER; \
        res.number = fnc(args[0].number); \
    } else if(args[0].type == OPT_INTEGER || args[0].type == OPT_BIGINT) { \
        res.type = OPT_NUMBER; \
        res.number = fnc(lv_int_num(&args[0]).number); \
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
DECL_MATH_FUNC(round);

#undef DECL_MATH_FUNC

static TextBufferObj abs_(TextBufferObj* args) {

    TextBufferObj res;
    getActualArgs(args, 1);
    if(args[0].type == OPT_NUMBER) {
        res.type = OPT_NUMBER;
        res.number = fabs(args[0].number);
    } else if(args[0].type == OPT_INTEGER) {
        // no overflow possible
        uint64_t a = args[0].integer;
        res.type = OPT_INTEGER;
        res.integer = isNegative(a) ? -a : a;
    } else if(args[0].type == OPT_BIGINT) {
        if(isNegative(args[0].bigint->data[args[0].bigint->len - 1])) {
            res = lv_int_neg(&args[0]);
        } else {
            res = args[0];
        }
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

static TextBufferObj atan2_(TextBufferObj* args) {

    TextBufferObj res;
    double a, b;
    getActualArgs(args, 2);
    if(args[0].type == OPT_INTEGER || args[0].type == OPT_BIGINT) {
        a = lv_int_num(&args[0]).number;
    } else if(args[0].type == OPT_NUMBER){
        a = args[0].number;
    } else {
        res.type = OPT_UNDEFINED;
        return res;
    }
    if(args[1].type == OPT_INTEGER || args[1].type == OPT_BIGINT) {
        b = lv_int_num(&args[1]).number;
    } else if(args[1].type == OPT_NUMBER){
        b = args[1].number;
    } else {
        res.type = OPT_UNDEFINED;
        return res;
    }
    res.type = OPT_NUMBER;
    res.number = atan2(a, b);
    return res;
}

/** Sign function. Returns +1 for +0 and -1 for -0. */
static TextBufferObj sgn(TextBufferObj* args) {

    TextBufferObj res;
    getActualArgs(args, 1);
    if(args[0].type == OPT_NUMBER) {
        res.type = OPT_INTEGER;
        // (-inf, -0] -> -1 : [+0, inf) -> +1
        res.integer = 1 - ((bool)signbit(args[0].number) << 1);
    } else if(args[0].type == OPT_INTEGER) {
        uint64_t a = args[0].integer;
        res.type = OPT_INTEGER;
        //[min, -1] -> -1, [0] -> 0, [1, max] -> +1
        res.integer = (a && (a < (UINT64_C(1) << 63))) - (a >> 63);
    } else if(args[0].type == OPT_BIGINT) {
        uint64_t a = args[0].bigint->data[args[0].bigint->len - 1];
        res.type = OPT_INTEGER;
        //[min, -1] -> -1, [0] -> 0, [1, max] -> +1
        res.integer = (a && (a < (UINT64_C(1) << 63))) - (a >> 63);
    } else {
        res.type = OPT_UNDEFINED;
    }
    return res;
}

TextBufferObj shl(TextBufferObj* args) {

    getActualArgs(args, 2);
    return indirect(args, args[0].type, ".shl");
}

TextBufferObj shr(TextBufferObj* args) {

    getActualArgs(args, 2);
    return indirect(args, args[0].type, ".shr");
}

//functional functions

/** Functional map */
static TextBufferObj map(TextBufferObj* args) {

    getActualArgs(args, 2);
    return indirect(args, args[0].type, ".map");
}

/** Functional filter */
static TextBufferObj filter(TextBufferObj* args) {

    getActualArgs(args, 2);
    return indirect(args, args[0].type, ".filter");
}

/** Functional fold */
static TextBufferObj fold(TextBufferObj* args) {

    getActualArgs(args, 3);
    return indirect(args, args[0].type, ".fold");
}

/** Slices the given vect or string */
static TextBufferObj slice(TextBufferObj* args) {

    getActualArgs(args, 3);
    return indirect(args, args[0].type, ".slice");
}

/** Takes elements from the vect while the predicate is satisfied */
TextBufferObj take(TextBufferObj* args) {

    getActualArgs(args, 2);
    return indirect(args, args[0].type, ".take");
}

/** Drops elements from the vect while the predicate is satisifed */
TextBufferObj skip(TextBufferObj* args) {

    getActualArgs(args, 2);
    return indirect(args, args[0].type, ".skip");
}
