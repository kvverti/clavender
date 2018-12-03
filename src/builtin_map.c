#include "builtin_funcs.h"
#include "builtin.h"
#include "lavender.h"
#include "textbuffer.h"
#include "expression.h"
#include <assert.h>
#include <string.h>

static void bsearchMap(TextBufferObj* map, TextBufferObj* key, TextBufferObj* res) {

    uint64_t h = lv_blt_hash(key);
    LvMapNode* lo = map->map->data;
    LvMapNode* hi = lo + map->map->len;
    while(lo < hi) {
        LvMapNode* mid = lo + (hi - lo) / 2;
        if(h == mid->hash) {
            //find the one of potential hash collisions
            for(LvMapNode* n = mid; n < hi && h == n->hash; n++) {
                if(lv_blt_equal(&n->key, key)) {
                    *res = n->value;
                    return;
                }
            }
            for(LvMapNode* n = mid; n > lo && h == n[-1].hash; n--) {
                if(lv_blt_equal(&n[-1].key, key)) {
                    *res = n[-1].value;
                    return;
                }
            }
            //no equal element found
            break;
        } else if(h < mid->hash) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    res->type = OPT_UNDEFINED;
}

#define INTRINSIC(name) \
    TextBufferObj lv_map_##name(TextBufferObj* args)

INTRINSIC(str) {
    assert(args[0].type == OPT_MAP);
    LvString* str;
    if(args[0].map->len == 0) {
        static char emptyMap[] = "{ }";
        str = lv_alloc(sizeof(LvString) + sizeof(emptyMap));
        str->refCount = 0;
        str->len = sizeof(emptyMap) - 1;
        memcpy(str->value, emptyMap, sizeof(emptyMap));
        return (TextBufferObj) {
            .type = OPT_STRING,
            .str = str
        };
    }
    size_t len = 2;
    str = lv_alloc(sizeof(LvString) + len + 1);
    str->refCount = 0;
    str->value[0] = '{';
    str->value[1] = ' ';
    str->value[2] = '\0';
    //concatenate keys and values
    for(size_t i = 0; i < args[0].map->len; i++) {
        LvString* tmp = lv_tb_getString(&args[0].map->data[i].key);
        len += tmp->len + 4;
        str = lv_realloc(str, sizeof(LvString) + len + 1);
        strcat(str->value, tmp->value);
        str->value[len - 4] = ' ';
        str->value[len - 3] = '=';
        str->value[len - 2] = '>';
        str->value[len - 1] = ' ';
        str->value[len] = '\0';
        if(tmp->refCount == 0)
            lv_free(tmp);
        tmp = lv_tb_getString(&args[0].map->data[i].value);
        len += tmp->len + 2;
        str = lv_realloc(str, sizeof(LvString) + len + 1);
        strcat(str->value, tmp->value);
        str->value[len - 2] = ',';
        str->value[len - 1] = ' ';
        str->value[len] = '\0';
        if(tmp->refCount == 0)
            lv_free(tmp);
    }
    str->value[len - 2] = ' ';
    str->value[len - 1] = '}';
    str->len = len;
    return (TextBufferObj) {
        .type = OPT_STRING,
        .str = str
    };
}

INTRINSIC(cat) {
    assert(args[0].type == OPT_MAP);
    if(args[1].type != OPT_MAP) {
        return (TextBufferObj) {
            .type = OPT_UNDEFINED
        };
    }
    size_t alen = args[0].map->len;
    size_t blen = args[1].map->len;
    LvMap* map = lv_alloc(sizeof(LvMap) + (alen + blen) * sizeof(LvMapNode));
    map->refCount = 0;
    map->len = alen + blen;
    for(size_t i = 0; i < alen; i++) {
        map->data[i] = args[0].map->data[i];
        INC_REFCOUNT(&map->data[i].key);
        INC_REFCOUNT(&map->data[i].value);
    }
    for(size_t i = 0; i < blen; i++) {
        map->data[alen + i] = args[1].map->data[i];
        INC_REFCOUNT(&map->data[alen + i].key);
        INC_REFCOUNT(&map->data[alen + i].value);
    }
    lv_tb_initMap(&map);
    TextBufferObj res;
    res.type = OPT_MAP;
    res.map = map;
    return res;
}

INTRINSIC(at) {
    assert(args[1].type == OPT_MAP);
    TextBufferObj res;
    //binary search the given key in the map
    bsearchMap(&args[1], &args[0], &res);
    return res;
}

INTRINSIC(eq) {
    assert(args[0].type == OPT_MAP);
    //currently maps are stored in sorted order, so pairwise
    //comparison is fine
    bool equal = true;
    if(args[1].type != OPT_MAP) {
        equal = false;
    } else if(args[0].map->len != args[1].map->len) {
        equal = false;
    } else {
        for(size_t i = 0; equal && i < args[0].map->len; i++) {
            LvMapNode* na = &args[0].map->data[i];
            LvMapNode* nb = &args[1].map->data[i];
            if(!lv_blt_equal(&na->key, &nb->key) || !lv_blt_equal(&na->value, &nb->value)) {
                equal = false;
            }
        }
    }
    return (TextBufferObj) {
        .type = OPT_INTEGER,
        .integer = equal
    };
}

INTRINSIC(lt) {
    assert(args[0].type == OPT_MAP);
    bool lt = false;
    if(args[1].type != OPT_MAP) {
        lt = args[0].type < args[1].type;
    } else if(args[0].map->len == args[1].map->len) {
        for(size_t i = 0; i < args[0].map->len; i++) {
            LvMapNode* na = &args[0].map->data[i];
            LvMapNode* nb = &args[1].map->data[i];
            if(lv_blt_lt(&na->key, &nb->key)) {
                lt = true;
                break;
            } else if(lv_blt_lt(&nb->key, &na->key)) {
                lt = false;
                break;
            } else if(lv_blt_lt(&na->value, &nb->value)) {
                lt = true;
                break;
            } else if(lv_blt_lt(&nb->value, &na->value)) {
                lt = false;
                break;
            }
        }
    } else {
        lt = args[0].map->len < args[1].map->len;
    }
    return (TextBufferObj) {
        .type = OPT_INTEGER,
        .integer = lt
    };
}

INTRINSIC(len) {
    assert(args[0].type = OPT_MAP);
    return (TextBufferObj) {
        .type = OPT_INTEGER,
        .integer = args[0].map->len
    };
}

INTRINSIC(map) {
    assert(args[0].type == OPT_MAP);
    TextBufferObj res;
    TextBufferObj func = args[1];
    LvMapNode* oldData = args[0].map->data;
    size_t len = args[0].map->len;
    LvMap* map = lv_alloc(sizeof(LvMap) + len * sizeof(LvMapNode));
    map->refCount = 0;
    map->len = len;
    for(size_t i = 0; i < len; i++) {
        TextBufferObj keyValue[2] = { oldData[i].key, oldData[i].value };
        TextBufferObj obj;
        lv_callFunction(&func, 2, keyValue, &obj);
        INC_REFCOUNT(&obj);
        map->data[i].value = obj;
        INC_REFCOUNT(&oldData[i].key);
        map->data[i].key = oldData[i].key;
        map->data[i].hash = oldData[i].hash;
    }
    res.type = OPT_MAP;
    res.map = map;
    return res;
}

INTRINSIC(filter) {
    TextBufferObj func = args[1];
    LvMapNode* oldData = args[0].map->data;
    size_t len = args[0].map->len;
    LvMap* map = lv_alloc(sizeof(LvMap) + len * sizeof(LvMapNode));
    map->refCount = 0;
    size_t newLen = 0;
    for(size_t i = 0; i < len; i++) {
        TextBufferObj keyValue[2] = { oldData[i].key, oldData[i].value };
        TextBufferObj passed;
        lv_callFunction(&func, 2, keyValue, &passed);
        INC_REFCOUNT(&passed);
        if(lv_blt_toBool(&passed)) {
            INC_REFCOUNT(&keyValue[0]);
            map->data[newLen].key = keyValue[0];
            map->data[newLen].hash = oldData[i].hash;
            INC_REFCOUNT(&keyValue[1]);
            map->data[newLen].value = keyValue[1];
            newLen++;
        }
        lv_expr_cleanup(&passed, 1);
    }
    map->len = newLen;
    if(newLen < len) {
        map = lv_realloc(map, sizeof(LvMap) + newLen * sizeof(LvMapNode));
    }
    TextBufferObj res;
    res.type = OPT_MAP;
    res.map = map;
    return res;
}

INTRINSIC(fold) {
    size_t len = args[0].map->len;
    LvMapNode* oldData = args[0].map->data;
    TextBufferObj accum[3] = { args[1] };
    TextBufferObj func = args[2];
    for(size_t i = 0; i < len; i++) {
        accum[1] = oldData[i].key;
        accum[2] = oldData[i].value;
        lv_callFunction(&func, 3, accum, &accum[0]);
    }
    return accum[0];
}
