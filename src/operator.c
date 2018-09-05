#include "operator.h"
#include "lavender.h"
#include "hashtable.h"
#include <string.h>
#include <assert.h>

static Hashtable funcNamespaces[FNS_COUNT];
//storing anonymous functions
static Operator* anonFuncs;

//destructor function for funcNamespaces
static void freeFuncNamespaces(char* key, void* value) {

    lv_free(key);
    Operator* op = (Operator*) value;
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

Operator* lv_op_getOperator(char* name, FuncNamespace ns) {

    assert(ns >= 0 && ns < FNS_COUNT);
    return lv_tbl_get(&funcNamespaces[ns], name);
}

Operator* lv_op_getScopedOperator(char* scope, char* name, FuncNamespace ns) {

    assert(ns >= 0 && ns < FNS_COUNT);
    size_t alen = strlen(scope);
    size_t blen =  strlen(name);
    char* fqn = lv_alloc(alen + 1 + blen + 1);
    memcpy(fqn, scope, alen);
    fqn[alen] = ':';
    memcpy(fqn + alen + 1, name, blen);
    fqn[alen + 1 + blen] = '\0';
    Operator* ret = lv_tbl_get(&funcNamespaces[ns], fqn);
    lv_free(fqn);
    return ret;
}

bool lv_op_addOperator(Operator* op, FuncNamespace ns) {

    assert(op);
    if(op->name[strlen(op->name) - 1] == ':') {
        //anonymous function
        op->next = anonFuncs;
        anonFuncs = op;
        return true;
    }
    return lv_tbl_put(&funcNamespaces[ns], op->name, op);
}

bool lv_op_removeOperator(char* name, FuncNamespace ns) {

    assert(ns >= 0 && ns < FNS_COUNT);
    Operator* removed = lv_tbl_del(&funcNamespaces[ns], name, NULL);
    if(removed)
        freeFuncNamespaces(removed->name, removed);
    return removed != NULL;
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
        lv_tbl_init(&funcNamespaces[i]);
    }
}

void lv_op_onShutdown(void) {

    freeList(anonFuncs);
    for(int i = 0; i < FNS_COUNT; i++) {
        lv_tbl_clear(&funcNamespaces[i], freeFuncNamespaces);
        lv_free(funcNamespaces[i].table);
    }
}
