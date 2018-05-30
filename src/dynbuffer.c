#include "dynbuffer.h"
#include "lavender.h"
#include <string.h>

void lv_buf_init(DynBuffer* self, size_t dataSize) {
    
    #define INIT_BUFFER_LEN 16
    self->dataSize = dataSize;
    self->data = lv_alloc(INIT_BUFFER_LEN * dataSize);
    self->cap = INIT_BUFFER_LEN;
    self->len = 0;
    #undef INIT_BUFFER_LEN
}

void lv_buf_push(DynBuffer* self, void* elem) {
    
    //tmp buffer, in case we need to realloc
    //aliasing ftw
    char tmp[self->dataSize];
    memcpy(tmp, elem, self->dataSize);
    if(self->len + 1 == self->cap) {
        self->data = lv_realloc(self->data, self->cap * 2 * self->dataSize);
        self->cap *= 2;
    }
    void* off = ((char*)self->data) + (self->len++ * self->dataSize);
    memcpy(off, tmp, self->dataSize);
}

void* lv_buf_pop(DynBuffer* self) {
    
    size_t off = --self->len * self->dataSize;
    return ((char*)self->data) + off;
}

void* lv_buf_get(DynBuffer* self, size_t idx) {
    
    return ((char*)self->data) + (idx * self->dataSize);
}
