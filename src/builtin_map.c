#include "builtin_funcs.h"
#include "lavender.h"
#include "textbuffer.h"
#include <assert.h>

#define INTRINSIC(name) \
    TextBufferObj lv_map_##name(TextBufferObj* args)

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
