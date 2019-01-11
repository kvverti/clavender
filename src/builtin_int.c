#include "builtin_funcs.h"
#include "bigint.h"
#include "textbuffer.h"
#include "lavender.h"
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#define INTRINSIC(name) \
    TextBufferObj lv_int_##name(TextBufferObj* args)

INTRINSIC(str) {
    TextBufferObj res = { .type = OPT_STRING };
    if(args[0].type == OPT_BIGINT) {
        #define LENGTH_RATIO 19.2659197224948L
        size_t len = 1 + (size_t)(args[0].bigint->len * LENGTH_RATIO);
        #undef LENGTH_RATIO
        LvString* s = lv_alloc(sizeof(LvString) + len + 2);
        s->refCount = 0;
        s->len = len + 1;
        len = yabi_toBuf(args[0].bigint, len + 1, s->value);
        s->value[len + 1] = '\0';
        s->value[len] = 'L';
        if(len != s->len) {
            s->len = len + 1;
            s = lv_realloc(s, sizeof(LvString) + len + 2);
        }
        assert(s->value[s->len] == '\0');
        res.str = s;
    } else {
        assert(args[0].type == OPT_INTEGER);
        uint64_t val = args[0].integer;
        //get the sign bit
        bool negative = NEGATIVE_INT(val);
        //get the magnitude of the value
        val = negative ? (-val) : val;
        //get the length (+1 for minus sign)
        size_t len = snprintf(NULL, 0, "%"PRIu64, val);
        LvString* s = lv_alloc(sizeof(LvString) + negative + len + 1);
        s->refCount = 0;
        s->len = negative + len;
        if(negative) {
            s->value[0] = '-';
        }
        snprintf(s->value + negative, len + 1, "%"PRIu64, val);
        res.str = s;
    }
    return res;
}

INTRINSIC(int) {
    assert(args[0].type == OPT_BIGINT || args[0].type == OPT_INTEGER);
    return args[0];
}

INTRINSIC(num) {
    TextBufferObj res = { .type = OPT_NUMBER };
    if(args[0].type == OPT_BIGINT) {
        char* str = yabi_toStr(args[0].bigint);
        double num = strtod(str, NULL);
        lv_free(str);
        res.number = num;
    } else {
        assert(args[0].type == OPT_INTEGER);
        int sign = -NEGATIVE_INT(args[0].integer);
        double num = (double)(sign ? -args[0].integer : args[0].integer);
        res.number = copysign(num, sign);
    }
    return res;
}

INTRINSIC(hash) {
    uint64_t h = 0;
    if(args[0].type == OPT_BIGINT) {
        for(size_t i = 0; i < args[0].bigint->len; i++) {
            h = h * 31 + args[0].bigint->data[i];
        }
    } else {
        assert(args[0].type == OPT_INTEGER);
        h = args[0].integer;
    }
    TextBufferObj res;
    res.type = OPT_INTEGER;
    res.integer = h;
    return res;
}

INTRINSIC(eq) {
    assert(args[0].type == args[1].type);
    TextBufferObj res = { .type = OPT_INTEGER };
    if(args[0].type == OPT_BIGINT) {
        res.integer = yabi_equal(args[0].bigint, args[1].bigint);
    } else {
        assert(args[0].type == OPT_INTEGER);
        res.integer = (args[0].integer == args[1].integer);
    }
    return res;
}

INTRINSIC(lt) {
    TextBufferObj res = { .type = OPT_INTEGER };
    if(args[0].type == args[1].type) {
        if(args[0].type == OPT_BIGINT) {
            res.integer = (yabi_cmp(args[0].bigint, args[1].bigint) < 0);
        } else {
            assert(args[0].type == OPT_INTEGER);
            int asign = NEGATIVE_INT(args[0].integer);
            int bsign = NEGATIVE_INT(args[1].integer);
            if(asign == bsign) {
                res.integer = (args[0].integer < args[1].integer);
            } else {
                res.integer = asign;
            }
        }
    } else {
        // get the signs
        bool abig = (args[0].type == OPT_BIGINT);
        int asign = NEGATIVE_INT(abig ? args[0].bigint->data[args[0].bigint->len - 1] : args[0].integer);
        int bsign = NEGATIVE_INT(!abig ? args[1].bigint->data[args[1].bigint->len - 1] : args[1].integer);
        if(asign == bsign) {
            res.integer = (abig == asign);
        } else {
            res.integer = asign;
        }
    }
    return res;
}

INTRINSIC(neg) {
    TextBufferObj res;
    if(args[0].type == OPT_BIGINT) {
        BigInt* bi = yabi_negate(args[0].bigint);
        if(bi->len == 1) {
            res.type = OPT_INTEGER;
            res.integer = bi->data[0];
            lv_free(bi);
        } else {
            res.type = OPT_BIGINT;
            res.bigint = bi;
        }
    } else {
        assert(args[0].type == OPT_INTEGER);
        if(args[0].integer == (UINT64_C(1) << 63)) {
            // int min special case
            BigInt* bi = YABI_NEW_BIGINT(2);
            bi->refCount = 0;
            bi->len = 2;
            bi->data[0] = -args[0].integer;
            bi->data[1] = -(WordType)NEGATIVE_INT(args[0].integer);
            res.type = OPT_BIGINT;
            res.bigint = bi;
        } else {
            res.type = OPT_INTEGER;
            res.integer = -args[0].integer;
        }
    }
    return res;
}

union FakeBigInt {
    BigInt bi;
    char a[sizeof(BigInt) + sizeof(WordType)];
};

#define TRY_BIGINT_ELSE(op) \
    bool aisbig = args[0].type == OPT_BIGINT; \
    bool bisbig = args[1].type == OPT_BIGINT; \
    if(aisbig || bisbig) { \
        BigInt* abi; \
        BigInt* bbi; \
        union FakeBigInt fakeBigint; \
        if(!aisbig) { \
            fakeBigint.bi.refCount = 0; \
            fakeBigint.bi.len = 1; \
            fakeBigint.bi.data[0] = args[0].integer; \
            abi = &fakeBigint.bi; \
        } else { \
            abi = args[0].bigint; \
        } \
        if(!bisbig) { \
            fakeBigint.bi.refCount = 0; \
            fakeBigint.bi.len = 1; \
            fakeBigint.bi.data[0] = args[1].integer; \
            bbi = &fakeBigint.bi; \
        } else { \
            bbi = args[1].bigint; \
        } \
        abi = yabi_##op(abi, bbi); \
        if(abi->len == 1) { \
            res.type = OPT_INTEGER; \
            res.integer = abi->data[0]; \
            lv_free(abi); \
        } else { \
            res.type = OPT_BIGINT; \
            res.bigint = abi; \
        } \
    } else

INTRINSIC(add) {
    TextBufferObj res;
    if(args[1].type == OPT_NUMBER) {
        TextBufferObj newArgs[2];
        newArgs[0] = lv_int_num(args);
        newArgs[1] = args[1];
        /* res = lv_num_add(newArgs); */
    } else {
        TRY_BIGINT_ELSE(add) {
            uint64_t a = args[0].integer;
            uint64_t b = args[1].integer;
            uint64_t u64 = a + b;
            if(NEGATIVE_INT(a) == NEGATIVE_INT(b) && NEGATIVE_INT(a) != NEGATIVE_INT(u64)) {
                // overflow :(
                BigInt* bi = YABI_NEW_BIGINT(2);
                bi->refCount = 0;
                bi->len = 2;
                bi->data[0] = u64;
                bi->data[1] = -NEGATIVE_INT(a);
                res.type = OPT_BIGINT;
                res.bigint = bi;
            } else {
                res.type = OPT_INTEGER;
                res.integer = u64;
            }
        }
    }
    return res;
}

INTRINSIC(sub) {
    TextBufferObj res;
    if(args[1].type == OPT_NUMBER) {
        TextBufferObj newArgs[2];
        newArgs[0] = lv_int_num(args);
        newArgs[1] = args[1];
        /* res = lv_num_sub(newArgs); */
    } else {
        TRY_BIGINT_ELSE(sub) {
            uint64_t a = args[0].integer;
            uint64_t b = args[1].integer;
            uint64_t u64 = a - b;
            if(NEGATIVE_INT(a) == !NEGATIVE_INT(b) && NEGATIVE_INT(a) != NEGATIVE_INT(u64)) {
                // overflow :(
                BigInt* bi = YABI_NEW_BIGINT(2);
                bi->refCount = 0;
                bi->len = 2;
                bi->data[0] = u64;
                bi->data[1] = -NEGATIVE_INT(a);
                res.type = OPT_BIGINT;
                res.bigint = bi;
            } else {
                res.type = OPT_INTEGER;
                res.integer = u64;
            }
        }
    }
    return res;
}

INTRINSIC(mul) {
    TextBufferObj res;
    if(args[1].type == OPT_NUMBER) {
        TextBufferObj newArgs[2];
        newArgs[0] = lv_int_num(args);
        newArgs[1] = args[1];
        // res = lv_num_mul(newArgs);
    } else {
        TRY_BIGINT_ELSE(mul) {
            #ifdef __GNUC__
            // use 128-bit math
            __uint128_t a = args[0].integer;
            __uint128_t b = args[1].integer;
            // sign extend
            a |= (-(__uint128_t)NEGATIVE_INT((uint64_t)a)) << 64;
            b |= (-(__uint128_t)NEGATIVE_INT((uint64_t)b)) << 64;
            __uint128_t u128 = a * b;
            // lower 63 bits are magnitude
            // remaining 65 bits are sign in a 64-bit word
            int sign = NEGATIVE_INT((uint64_t)a) ^ NEGATIVE_INT((uint64_t)b);
            if((u128 >> 63) != (-(__uint128_t)sign >> 63) && u128 != 0) {
                BigInt* bi = YABI_NEW_BIGINT(2);
                bi->refCount = 0;
                bi->len = 2;
                bi->data[0] = u128;
                bi->data[1] = u128 >> 64;
                res.type = OPT_BIGINT;
                res.bigint = bi;
            } else {
                res.type = OPT_INTEGER;
                res.integer = u128;
            }
            #else
            // use BigInt multiplcation
            union FakeBigInt fa, fb;
            fa.bi.len = 1;
            fb.bi.len = 1;
            fa.bi.data[0] = args[0].integer;
            fb.bi.data[0] = args[1].integer;
            uint64_t result[3] = { 0, 0, 0 };
            size_t len = yabi_mulToBuf(&fa.bi, &fb.bi, 3, result);
            if(len != 1) {
                BigInt* bi = YABI_NEW_BIGINT(len);
                bi->refCount = 0;
                bi->len = len;
                memcpy(bi->data, result, len * sizeof(uint64_t));
                res.type = OPT_BIGINT;
                res.bigint = bi;
            } else {
                res.type = OPT_INTEGER;
                res.integer = result[0];
            }
            #endif
        }
    }
    return res;
}
