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

static OpHashtable funcNamespaces[FNS_COUNT];
//storing anonymous functions
static Operator* anonFuncs;

static void resizeTable(OpHashtable* table);
static void freeOp(Operator* op);

static size_t hashInit(size_t init, char* str) {
    //djb2 hash
    size_t res = init;
    int c;
    while((c = *str++))
        res = ((res << 5) + res) + c;
    return res;
}

static size_t hash(char* str) {

    return hashInit(5381, str);
}

static size_t hashQual(char* scope, char* name) {

    static char* col = ":";
    size_t res = hash(scope);
    res = hashInit(res, col);
    res = hashInit(res, name);
    return res;
}

static bool qualNameEq(char* name1, char* scope, char* name2) {

    size_t len = strlen(scope);
    if(strncmp(name1, scope, len) != 0)
        return false;
    if(name1[len] != ':')
        return false;
    if(strcmp(name1 + len + 1, name2) != 0)
        return false;
    return true;
}

Operator* lv_op_getOperator(char* name, FuncNamespace ns) {

    assert(ns >= 0 && ns < FNS_COUNT);
    OpHashtable* table = &funcNamespaces[ns];
    //hash(name) % cap
    size_t idx = hash(name) & (table->cap - 1);
    Operator* head = table->table[idx];
    while(head && strcmp(name, head->name) != 0)
        head = head->next;
    return head;
}

Operator* lv_op_getScopedOperator(char* scope, char* name, FuncNamespace ns) {

    assert(ns >= 0 && ns < FNS_COUNT);
    OpHashtable* table = &funcNamespaces[ns];
    size_t idx = hashQual(scope, name) & (table->cap - 1);
    Operator* head = table->table[idx];
    while(head && !qualNameEq(head->name, scope, name) != 0)
        head = head->next;
    return head;
}

bool lv_op_addOperator(Operator* op, FuncNamespace ns) {

    assert(op);
    if(op->name[strlen(op->name) - 1] == ':') {
        //anonymous function
        op->next = anonFuncs;
        anonFuncs = op;
        return true;
    }
    OpHashtable* table = &funcNamespaces[ns];
    //check table load
    if(((double) table->size / table->cap) > TABLE_LOAD_FACT)
        resizeTable(table);
    size_t idx = hash(op->name) & (table->cap - 1);
    Operator* head = table->table[idx];
    Operator* tmp = head;
    while(tmp && strcmp(op->name, tmp->name) != 0)
        tmp = tmp->next;
    if(tmp) {
        //duplicate
        return false;
    } else {
        op->next = head;
        table->table[idx] = op;
        table->size++;
        return true;
    }
}

bool lv_op_removeOperator(char* name, FuncNamespace ns) {
    //the "triple ref" pointer technique
    assert(ns >= 0 && ns < FNS_COUNT);
    OpHashtable* table = &funcNamespaces[ns];
    size_t idx = hash(name) & (table->cap - 1);
    Operator** tmp = &table->table[idx];
    while(*tmp && strcmp(name, (*tmp)->name) != 0) {
        tmp = &(*tmp)->next;
    }
    Operator* toFree = *tmp;
    if(toFree) {
        *tmp = toFree->next;
        freeOp(toFree);
        table->size--;
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
            lv_op_addOperator(node, table - funcNamespaces);
            node = tmp;
        }
    }
    lv_free(oldTable);
}

static void freeOp(Operator* op) {

    lv_free(op->name);
    if(op->type == FUN_FWD_DECL) {
        //free param names
        Param* params = op->params;
        int arity = op->arity + op->locals;
        for(int i = 0; i < arity; i++) {
            lv_free(params[i].name);
        }
        lv_free(params);
    }
    lv_free(op);
}

static void freeList(Operator* head) {

    while(head) {
        Operator* tmp = head->next;
        freeOp(head);
        head = tmp;
    }
}

void lv_op_onStartup(void) {

    for(int i = 0; i < FNS_COUNT; i++) {
        funcNamespaces[i].table = lv_alloc(INIT_TABLE_LEN * sizeof(Operator*));
        memset(funcNamespaces[i].table, 0, INIT_TABLE_LEN * sizeof(Operator*));
        funcNamespaces[i].size = 0;
        funcNamespaces[i].cap = INIT_TABLE_LEN;
    }
}

void lv_op_onShutdown(void) {

    freeList(anonFuncs);
    for(int i = 0; i < FNS_COUNT; i++) {
        for(size_t j = 0; j < funcNamespaces[i].cap; j++)
            freeList(funcNamespaces[i].table[j]);
        lv_free(funcNamespaces[i].table);
    }
}
