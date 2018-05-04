#include "operator.h"
#include "lavender.h"
#include <string.h>
#include <assert.h>

//the hashtable size. Should be a power of 2
#define TABLE_LEN 32
static Operator* prefixHashtable[TABLE_LEN];
//storing anonymous functions
static Operator* anonFuncs;

static size_t hash(char* str) {
    //djb2 hash
    size_t res = 5381;
    int c;
    while((c = *str++))
        res = ((res << 5) + res) + c;
    return res;
}

Operator* lv_op_getOperator(char* name) {
    
    //hash(name) % TABLE_LEN
    size_t idx = hash(name) & (TABLE_LEN - 1);
    Operator* head = prefixHashtable[idx];
    while(head && strcmp(name, head->name) != 0)
        head = head->next;
    return head;
}

bool lv_op_addOperator(Operator* op) {
    
    assert(op);
    if(!op->name) {
        op->next = anonFuncs;
        anonFuncs = op;
        return true;
    }
    size_t idx = hash(op->name) & (TABLE_LEN - 1);
    Operator* head = prefixHashtable[idx];
    Operator* tmp = head;
    while(tmp && strcmp(op->name, tmp->name) != 0)
        tmp = tmp->next;
    if(tmp) {
        //duplicate
        return false;
    } else {
        op->next = head;
        prefixHashtable[idx] = op;
        return true;
    }
}

bool lv_op_removeOperator(char* name) {
    //the "triple ref" pointer technique
    size_t idx = hash(name) & (TABLE_LEN - 1);
    Operator** tmp = &prefixHashtable[idx];
    while(*tmp && strcmp(name, (*tmp)->name) != 0) {
        tmp = &(*tmp)->next;
    }
    Operator* toFree = *tmp;
    if(toFree) {
        *tmp = toFree->next;
        lv_free(toFree);
        return true;
    }
    return false;
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

void lv_op_onShutdown() {
    
    freeList(anonFuncs);
    for(int i = 0; i < TABLE_LEN; i++)
        freeList(prefixHashtable[i]);
}
