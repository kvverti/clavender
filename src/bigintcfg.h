#ifndef LV_BIGINT_CFG_H
#define LV_BIGINT_CFG_H
#include <stddef.h>

extern void* lv_alloc(size_t len);
extern void* lv_realloc(void* p, size_t len);
extern void lv_free(void* p);

#define YABI_WORD_BIT_SIZE 64
#define YABI_MALLOC(s) (lv_alloc(s))
#define YABI_REALLOC(p, s) (lv_realloc(p, s))
#define YABI_FREE(p) (lv_free(p))

#endif
