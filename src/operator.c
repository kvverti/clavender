#include "operator.h"
#include "lavender.h"
#include <string.h>
#include <assert.h>

//the hashtable size. Should be a power of 2
#define INIT_TABLE_LEN 64
#define TABLE_LOAD_FACT 0.75
typedef struct OpHashtable {
    size_t size;
    size_t cap;
    Operator** table;
} OpHashtable;

static OpHashtable prefixFuncs;
static OpHashtable infixFuncs;
//storing anonymous functions
static Operator* anonFuncs;

static void resizeTable(OpHashtable* table);

static size_t hash(char* str) {
    //djb2 hash
    size_t res = 5381;
    int c;
    while((c = *str++))
        res = ((res << 5) + res) + c;
    return res;
}

Operator* lv_op_getOperator(char* name) {
    
    //hash(name) % cap
    size_t idx = hash(name) & (prefixFuncs.cap - 1);
    Operator* head = prefixFuncs.table[idx];
    while(head && strcmp(name, head->name) != 0)
        head = head->next;
    return head;
}

bool lv_op_addOperator(Operator* op) {
    
    assert(op);
    if(!op->name) {
        //anonymous function
        op->next = anonFuncs;
        anonFuncs = op;
        return true;
    }
    //check table load
    if(((double) prefixFuncs.size / prefixFuncs.cap) > TABLE_LOAD_FACT)
        resizeTable(&prefixFuncs);
    size_t idx = hash(op->name) & (prefixFuncs.cap - 1);
    Operator* head = prefixFuncs.table[idx];
    Operator* tmp = head;
    while(tmp && strcmp(op->name, tmp->name) != 0)
        tmp = tmp->next;
    if(tmp) {
        //duplicate
        return false;
    } else {
        op->next = head;
        prefixFuncs.table[idx] = op;
        prefixFuncs.size++;
        return true;
    }
}

bool lv_op_removeOperator(char* name) {
    //the "triple ref" pointer technique
    size_t idx = hash(name) & (prefixFuncs.cap - 1);
    Operator** tmp = &prefixFuncs.table[idx];
    while(*tmp && strcmp(name, (*tmp)->name) != 0) {
        tmp = &(*tmp)->next;
    }
    Operator* toFree = *tmp;
    if(toFree) {
        *tmp = toFree->next;
        lv_free(toFree);
        prefixFuncs.size--;
        return true;
    }
    return false;
}

static void resizeTable(OpHashtable* table) {
    
    Operator** oldTable = table->table;
    size_t oldCap = table->cap;
    table->table = lv_alloc(oldCap * 2 * sizeof(Operator*));
    memset(table->table, 0, oldCap * 2 * sizeof(Operator*));
    table->cap *= 2;
    table->size = 0;
    for(int i = 0; i < oldCap; i++) {
        Operator* node = oldTable[i];
        while(node) {
            Operator* tmp = node->next;
            lv_op_addOperator(node);
            node = tmp;
        }
    }
    lv_free(oldTable);
}

static void freeList(Operator* head) {
    
    while(head) {
        Operator* tmp = head->next;
        lv_free(head->name);
        if(head->type == OPT_FWD_DECL)
            lv_free(head->decl);
        lv_free(head);
        head = tmp;
    }
}

void lv_op_onStartup() {
    
    prefixFuncs.table = lv_alloc(INIT_TABLE_LEN * sizeof(Operator*));
    memset(prefixFuncs.table, 0, INIT_TABLE_LEN * sizeof(Operator*));
    prefixFuncs.size = 0;
    prefixFuncs.cap = INIT_TABLE_LEN;
}

void lv_op_onShutdown() {
    
    freeList(anonFuncs);
    for(int i = 0; i < prefixFuncs.cap; i++)
        freeList(prefixFuncs.table[i]);
    lv_free(prefixFuncs.table);
}
