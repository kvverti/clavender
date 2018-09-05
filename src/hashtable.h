#ifndef HASHTABLE_H
#define HASHTABLE_H
#include <stddef.h>
#include <stdbool.h>

typedef struct HashNode HashNode;
typedef struct Hashtable {
    size_t len;
    size_t cap;
    HashNode** table;
} Hashtable;

/**
 * Initializes the table with the given key and value sizes and to use
 * the given hash and key compare functions.
 */
void lv_tbl_init(Hashtable* table);

/**
 * Returns the value associated with the given key, or NULL if none such exists.
 * The value is owned by the hashtable.
 */
void* lv_tbl_get(Hashtable* table, char* key);

/**
 * Determines whether the given key is already in the table; if not,
 * associates the key with the given value. Returns whether the value
 * was added. Unlike DynBuffer, the value is not copied, but the pointer
 * is stored directly.
 */
bool lv_tbl_put(Hashtable* table, char* key, void* value);

/**
 * Deletes the value associated with the given key from the table.
 * Returns the old value, or NULL if there was no old value. The key
 * originally stored in the table is returned through `oldKey` if not
 * NULL.
 */
void* lv_tbl_del(Hashtable* table, char* key, char** oldKey);

/**
 * Clears all elements from the given table using the given
 * callback (if not NULL) to clean up old keys and values.
 */
void lv_tbl_clear(Hashtable* table, void (*cb)(char*, void*));


#endif
