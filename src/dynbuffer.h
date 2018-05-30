#ifndef STACK_H
#define STACK_H
#include <stddef.h>

/**
 * A simple type-agnostic dynamic buffer.
 */
typedef struct DynBuffer {
    void* data;
    size_t cap;
    size_t len;
    size_t dataSize; //size of a data element
} DynBuffer;

void lv_buf_init(DynBuffer* self, size_t dataSize);
void lv_buf_push(DynBuffer* self, void* elem);
void* lv_buf_pop(DynBuffer* self);
void* lv_buf_get(DynBuffer* self, size_t idx);

#endif
