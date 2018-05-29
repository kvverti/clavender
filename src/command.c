#include "command.h"
#include "lavender.h"
#include "operator.h"
#include <string.h>
#include <assert.h>

typedef struct CommandElement {
    char* name;
    bool (*run)(Token*);
} CommandElement;

static bool quit(Token* head);
static bool import(Token* head);
static bool using(Token* head);

static CommandElement COMMANDS[] = {
    { "quit", quit },
    { "import", import },
    { "using", using },
};
#define NUM_COMMANDS (sizeof(COMMANDS) / sizeof(CommandElement))

bool lv_cmd_run(Token* head) {
    
    if(!head || head->type != TTY_IDENT) {
        lv_cmd_message = "Not a command";
        return false;
    }
    for(size_t i = 0; i < NUM_COMMANDS; i++) {
        if(strcmp(head->value, COMMANDS[i].name) == 0) {
            return COMMANDS[i].run(head);
        }
    }
    lv_cmd_message = "No command found";
    return false;
}

static bool quit(Token* head) {
    
    lv_tkn_free(head);
    lv_shutdown();
    return true; //never reached
}

//string to string hashtable for storing
//imported file names and imported function mappings
#define INIT_TABLE_LEN 64 //must be power of two
#define TABLE_LOAD_FACT 0.75
typedef struct StrHashNode {
    //key and value must be dynamically allocated
    char* key;
    char* value;
    struct StrHashNode* next;
} StrHashNode;

typedef struct StrHashtable {
    size_t size;
    size_t cap;
    StrHashNode** table;
} StrHashtable;

static char* tableGet(StrHashtable* table, char* key);
static bool tableAdd(StrHashtable* table, char* key, char* value);
static bool tableRemove(StrHashtable* table, char* key);
static void tableClear(StrHashtable* table);
static void tableResize(StrHashtable* table);
static void nodeFree(StrHashNode* node);

static size_t hash(char* str) {
    //djb2 hash
    size_t res = 5381;
    int c;
    while((c = *str++))
        res = ((res << 5) + res) + c;
    return res;
}

static char* tableGet(StrHashtable* table, char* key) {
    
    size_t idx = hash(key) & (table->cap - 1);
    StrHashNode* head = table->table[idx];
    while(head && strcmp(key, head->key) != 0)
        head = head->next;
    return head ? head->value : NULL;
}

static bool tableAdd(StrHashtable* table, char* key, char* value) {
    
    if(((double) table->size / table->cap) > TABLE_LOAD_FACT)
        tableResize(table);
    size_t idx = hash(key) & (table->cap - 1);
    StrHashNode* head = table->table[idx];
    StrHashNode* tmp = head;
    while(tmp && strcmp(key, tmp->key) != 0)
        tmp = tmp->next;
    if(tmp) {
        return false;
    } else {
        tmp = lv_alloc(sizeof(StrHashNode));
        tmp->key = key;
        tmp->value = value;
        tmp->next = head;
        table->table[idx] = tmp;
        table->size++;
        return true;
    }
}

static bool tableRemove(StrHashtable* table, char* key) {
    
    size_t idx = hash(key) & (table->cap - 1);
    StrHashNode** tmp = &table->table[idx];
    while(*tmp && strcmp(key, (*tmp)->key) != 0)
        tmp = &(*tmp)->next;
    StrHashNode* toFree = *tmp;
    if(toFree) {
        *tmp = toFree->next;
        nodeFree(toFree);
        table->size--;
        return true;
    }
    return false;
}

static void tableClear(StrHashtable* table) {
    
    size_t cap = table->cap;
    for(size_t i = 0; i < cap; i++) {
        StrHashNode* node = table->table[i];
        while(node) {
            StrHashNode* tmp = node->next;
            nodeFree(node);
            node = tmp;
        }
    }
    memset(table->table, 0, cap * sizeof(StrHashNode*));
    table->size = 0;
}

static void tableResize(StrHashtable* table) {
    
    StrHashNode** oldTable = table->table;
    size_t oldCap = table->cap;
    table->table = lv_alloc(oldCap * 2 * sizeof(StrHashNode*));
    memset(table->table, 0, oldCap * 2 * sizeof(StrHashNode*));
    table->cap *= 2;
    table->size = 0;
    for(size_t i = 0; i < oldCap; i++) {
        StrHashNode* node = oldTable[i];
        while(node) {
            tableAdd(table, node->key, node->value);
            StrHashNode* tmp = node->next;
            lv_free(node);
            node = tmp;
        }
    }
    lv_free(oldTable);
}

static void nodeFree(StrHashNode* node) {
    
    lv_free(node->key);
    lv_free(node->value);
    lv_free(node);
}

static StrHashtable importNames;
static StrHashtable usingNames;

//number of imported scopes should be small
#define INIT_NUM_SCOPES 8
static struct Scopes {
    char** data;
    size_t cap;
    size_t len;
} nameScopes;

void lv_cmd_onStartup(void) {
    
    importNames.table = lv_alloc(INIT_TABLE_LEN * sizeof(StrHashNode*));
    memset(importNames.table, 0, INIT_TABLE_LEN * sizeof(StrHashNode*));
    importNames.cap = INIT_TABLE_LEN;
    importNames.size = 0;
    usingNames.table = lv_alloc(INIT_TABLE_LEN * sizeof(StrHashNode*));
    memset(usingNames.table, 0, INIT_TABLE_LEN * sizeof(StrHashNode*));
    usingNames.cap = INIT_TABLE_LEN;
    usingNames.size = 0;
    nameScopes.data = lv_alloc(INIT_NUM_SCOPES * sizeof(char*));
    nameScopes.cap = INIT_NUM_SCOPES;
    nameScopes.len = 0;
}

void lv_cmd_onShutdown(void) {
    
    for(size_t i = 0; i < nameScopes.len; i++) {
        lv_free(nameScopes.data[i]);
    }
    lv_free(nameScopes.data);
    tableClear(&importNames);
    tableClear(&usingNames);
    lv_free(importNames.table);
    lv_free(usingNames.table);
}

//end hashtable impl

char* lv_cmd_getQualNameFor(char* simpleName) {
    
    return tableGet(&usingNames, simpleName);
}

void addScope(char* sc) {
    
    if(nameScopes.len + 1 == nameScopes.cap) {
        nameScopes.data = lv_realloc(nameScopes.data, nameScopes.cap * 2 * sizeof(char*));
        nameScopes.cap *= 2;
    }
    nameScopes.data[nameScopes.len++] = sc;
}

void lv_cmd_getUsingScopes(char*** scopes, size_t* len) {
    
    *scopes = nameScopes.data;
    *len = nameScopes.len;
}

static bool using(Token* head) {
    
    head = head->next;
    if(!head || head->next) {
        lv_cmd_message = "Usage: @using <name(space)>";
        return false;
    }
    switch(head->type) {
        case TTY_IDENT: {
            //using namespace
            char* scope;
            {
                size_t len = strlen(head->value) + 1;
                scope = lv_alloc(len);
                memcpy(scope, head->value, len);
            }
            addScope(scope);
            lv_cmd_message = "Using successful";
            return true;
        }
        case TTY_QUAL_IDENT:
        case TTY_QUAL_SYMBOL: {
            //using specific
            Operator* op = NULL;
            for(FuncNamespace ns = 0; (ns < FNS_COUNT) && !op; ns++) {
                op = lv_op_getOperator(head->value, ns);
            }
            if(!op) {
                lv_cmd_message = "Error: name not found";
                return false;
            }
            char* qualName;
            {
                size_t len = strlen(head->value) + 1;
                qualName = lv_alloc(len);
                memcpy(qualName, head->value, len);
            }
            char* simpleName;
            {
                char* tmp = strchr(qualName, ':') + 1;
                size_t len = strlen(tmp) + 1;
                simpleName = lv_alloc(len);
                memcpy(simpleName, tmp, len);
            }
            tableAdd(&usingNames, simpleName, qualName);
            lv_cmd_message = "Using successful";
            return true;
        }
        default:
            lv_cmd_message = "Error: not a valid name";
            return false;
    }
    assert(false);
}

static bool import(Token* head) {
    
    head = head->next;
    if(!head || head->next) {
        lv_cmd_message = "Usage: @import <file>";
        return false;
    }
    if(head->type != TTY_IDENT) {
        lv_cmd_message = "Error: Invalid argument format";
        return false;
    }
    if(lv_readFile(head->value)) {
        lv_cmd_message = "Import successful";
        return true;
    } else {
        lv_cmd_message = "Import failed";
        return false;
    }
}
