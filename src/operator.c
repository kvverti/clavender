#include "operator.h"
#include "lavender.h"
#include <string.h>
#include <assert.h>

char* lv_op_getError(OpError error) {
    #define LEN 11
    static char* msg[LEN] = {
        "Expr does not define a function",
        "Reached end of input while parsing",
        "Expected an argument list",
        "Malformed argument list",
        "Missing function body",
        "Duplicate function definition",
        "Function name not found",
        "Expected operator",
        "Expected operand",
        "Encountered unexpected token",
        "Unbalanced parens or brackets"
    };
    assert(error > 0 && error <= LEN);
    return msg[error - 1];
    #undef LEN
}

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

static size_t hash(char* str) {
    //djb2 hash
    size_t res = 5381;
    int c;
    while((c = *str++))
        res = ((res << 5) + res) + c;
    return res;
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
        int arity = op->arity;
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

LvString* lv_op_getString(TextBufferObj* obj) {
    
    LvString* res;
    switch(obj->type) {
        case OPT_STRING: { //do we really need to copy string?
            size_t sz = sizeof(LvString) + obj->str->len + 1;
            res = lv_alloc(sz);
            memcpy(res, obj->str, sz);
            return res;
        }
        case OPT_NUMBER: {
            //because rather nontrivial to find the length of a
            //floating-point value before putting it into a string,
            //we first make a reasonable (read: arbitrary) guess as
            //to the buffer length required. Then, we take the actual
            //number of characters printed by snprintf and reallocate
            //the buffer to that length (plus 1 for the terminator).
            //If our initial guess was to little, we call snprintf
            //again in order to get all the characters.
            #define GUESS_LEN 16
            res = lv_alloc(sizeof(LvString) + GUESS_LEN); //first guess
            int len = snprintf(res->value, GUESS_LEN, "%g", obj->number);
            res = lv_realloc(res, sizeof(LvString) + len + 1);
            if(len >= GUESS_LEN) {
                snprintf(res->value, len + 1, "%g", obj->number);
            }
            res->len = len;
            return res;
            #undef GUESS_LEN
        }
        case OPT_FUNCTION:
        case OPT_FUNCTION_VAL: {
            size_t len = strlen(obj->func->name);
            res = lv_alloc(sizeof(LvString) + len + 1);
            res->len = len;
            strcpy(res->value, obj->func->name);
            return res;
        }
        default: {
            static char str[] = "<internal operator>";
            res = lv_alloc(sizeof(LvString) + sizeof(str));
            res->len = sizeof(str) - 1;
            strcpy(res->value, str);
            return res;
        }
    }
}

void lv_op_onStartup() {
    
    for(int i = 0; i < FNS_COUNT; i++) {
        funcNamespaces[i].table = lv_alloc(INIT_TABLE_LEN * sizeof(Operator*));
        memset(funcNamespaces[i].table, 0, INIT_TABLE_LEN * sizeof(Operator*));
        funcNamespaces[i].size = 0;
        funcNamespaces[i].cap = INIT_TABLE_LEN;
    }
}

void lv_op_onShutdown() {
    
    freeList(anonFuncs);
    for(int i = 0; i < FNS_COUNT; i++) {
        for(size_t j = 0; j < funcNamespaces[i].cap; j++)
            freeList(funcNamespaces[i].table[j]);
        lv_free(funcNamespaces[i].table);
    }
}
