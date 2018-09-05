#include "hashtable.h"
#include "lavender.h"
#include <string.h>

#define INIT_TABLE_LEN 64 //must be power of two
#define TABLE_LOAD_FACT 0.75
struct HashNode {
    //key and value must be dynamically allocated
    char* key;
    void* value;
    HashNode* next;
};

static void resize(Hashtable* table);

static size_t hash(char* str) {
    //djb2 hash
    size_t res = 5381;
    int c;
    while((c = *str++))
        res = ((res << 5) + res) + c;
    return res;
}

void lv_tbl_init(Hashtable* table) {

    table->len = 0;
    table->cap = INIT_TABLE_LEN;
    table->table = lv_alloc(INIT_TABLE_LEN * sizeof(HashNode*));
    memset(table->table, 0, INIT_TABLE_LEN * sizeof(HashNode*));
}

void* lv_tbl_get(Hashtable* table, char* key) {

    size_t idx = hash(key) & (table->cap - 1);
    HashNode* head = table->table[idx];
    while(head && strcmp(key, head->key) != 0)
        head = head->next;
    return head ? head->value : NULL;
}

bool lv_tbl_put(Hashtable* table, char* key, void* value) {

    if(((double) table->len / table->cap) > TABLE_LOAD_FACT)
        resize(table);
    size_t idx = hash(key) & (table->cap - 1);
    HashNode* head = table->table[idx];
    HashNode* tmp = head;
    while(tmp && strcmp(key, tmp->key) != 0)
        tmp = tmp->next;
    if(tmp) {
        return false;
    } else {
        tmp = lv_alloc(sizeof(HashNode));
        tmp->key = key;
        tmp->value = value;
        tmp->next = head;
        table->table[idx] = tmp;
        table->len++;
        return true;
    }
}

void* lv_tbl_del(Hashtable* table, char* key, char** oldKey) {

    size_t idx = hash(key) & (table->cap - 1);
    HashNode** tmp = &table->table[idx];
    while(*tmp && strcmp(key, (*tmp)->key) != 0)
        tmp = &(*tmp)->next;
    HashNode* toFree = *tmp;
    void* ret;
    if(toFree) {
        *tmp = toFree->next;
        if(oldKey)
            *oldKey = toFree->key;
        ret = toFree->value;
        table->len--;
        lv_free(toFree);
    } else {
        if(oldKey)
            *oldKey = NULL;
        ret = NULL;
    }
    return ret;
}

void lv_tbl_clear(Hashtable* table, void (*cb)(char*, void*)) {

    size_t cap = table->cap;
    for(size_t i = 0; i < cap; i++) {
        HashNode* node = table->table[i];
        while(node) {
            if(cb)
                cb(node->key, node->value);
            HashNode* tmp = node->next;
            lv_free(node);
            node = tmp;
        }
    }
    memset(table->table, 0, cap * sizeof(HashNode*));
    table->len = 0;
}

static void resize(Hashtable* table) {

    HashNode** oldTable = table->table;
    size_t oldCap = table->cap;
    table->table = lv_alloc(oldCap * 2 * sizeof(HashNode*));
    memset(table->table, 0, oldCap * 2 * sizeof(HashNode*));
    table->cap *= 2;
    table->len = 0;
    for(size_t i = 0; i < oldCap; i++) {
        HashNode* node = oldTable[i];
        while(node) {
            lv_tbl_put(table, node->key, node->value);
            HashNode* tmp = node->next;
            lv_free(node);
            node = tmp;
        }
    }
    lv_free(oldTable);
}
