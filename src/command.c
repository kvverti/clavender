#include "command.h"
#include "lavender.h"
#include "operator.h"
#include "hashtable.h"
#include <string.h>
#include <assert.h>

typedef struct CommandElement {
    char* name;
    bool (*run)(Token*);
} CommandElement;

static bool quit(Token* head);
static bool import(Token* head);

static CommandElement COMMANDS[] = {
    { "quit", quit },
    { "import", import },
};
#define NUM_COMMANDS (sizeof(COMMANDS) / sizeof(CommandElement))

bool lv_cmd_run(Token* head) {

    if(!head || head->type != TTY_IDENT) {
        lv_cmd_message = "Not a command";
        return false;
    }
    for(size_t i = 0; i < NUM_COMMANDS; i++) {
        if(lv_tkn_cmp(head, COMMANDS[i].name) == 0) {
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

//dyn alloc keys and vals
static Hashtable usingNames;

//destructor function for usingNames data
static void freeUsingNames(char* key, void* value) {

    lv_free(key);
    lv_free(value);
}

//number of imported scopes should be small
#define INIT_NUM_SCOPES 8
static struct Scopes {
    char** data;
    size_t cap;
    size_t len;
} nameScopes;

static void initNameScopes(void) {

    nameScopes.data = lv_alloc(INIT_NUM_SCOPES * sizeof(char*));
    nameScopes.cap = INIT_NUM_SCOPES;
    nameScopes.len = 0;
}

static void freeNameScopes(void) {

    for(size_t i = 0; i < nameScopes.len; i++) {
        lv_free(nameScopes.data[i]);
    }
    lv_free(nameScopes.data);
}

void lv_cmd_onStartup(void) {

    lv_tbl_init(&usingNames);
    initNameScopes();
}

void lv_cmd_onShutdown(void) {

    freeNameScopes();
    lv_tbl_clear(&usingNames, freeUsingNames);
    lv_free(usingNames.table);
}

char* lv_cmd_getQualNameFor(char* simpleName) {

    return lv_tbl_get(&usingNames, simpleName);
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

static inline bool isIdent(TokenType type) {

    return type == TTY_IDENT || type == TTY_SYMBOL;
}

/**
 * Gets the alias from head, returning it, and storing the next
 * unread token in end. Returns false if a problem occurred.
 */
static bool getAlias(Token* head, Token** original, Token** alias, Token** end) {

    //pieces in the form of <id> [as <alias>]
    assert(end);
    if(isIdent(head->type)) {
        //declared id is the default alias
        *original = head;
        *alias = head;
        head = head->next;
        if(head) {
            if(lv_tkn_cmp(head, "as") == 0) {
                head = head->next;
                //this token had better be an alias
                if(!head || !isIdent(head->type)) {
                    lv_cmd_message = "Error: expected alias";
                    return false;
                }
                //new alias specified
                *alias = head;
                head = head->next;
            }
        }
        *end = head;
        return true;
    } else {
        lv_cmd_message = "Error: unexpected token";
        return false;
    }
}

static bool wildcardImport(Token* nspace, Token* head, Token** end) {
    //using everything!
    char* scope;
    {
        size_t len = nspace->len;
        scope = lv_alloc(len + 1);
        memcpy(scope, nspace->start, len);
        scope[len] = '\0';
    }
    addScope(scope);
    if(!head) {
        //nothing else specified
        *end = head;
        return true;
    } else if(head->value[0] != ',') {
        lv_cmd_message = "Error: unexpected token";
        return false;
    }
    head = head->next;
    if(!head) {
        lv_cmd_message = "Error: expected alias";
        return false;
    }
    *end = head;
    return true;
}

/** Adds an alias `alias` for the function `<nspace>:<original>` */
static bool addFunctionAlias(Token* nspace, Token* original, Token* alias) {

    char* qualName;
    {
        size_t alen = nspace->len;
        size_t blen = original->len;
        qualName = lv_alloc(alen + 1 + blen + 1);
        memcpy(qualName, nspace->start, alen);
        qualName[alen] = ':';
        memcpy(qualName + alen + 1, original->start, blen);
        qualName[alen + 1 + blen] = '\0';
        //fix colons in names
        char* c = qualName + alen + 1;
        while((c = strchr(c, ':'))) {
            *c = '#';
        }
    }
    Operator* op = NULL;
    for(FuncNamespace ns = 0; (ns < FNS_COUNT) && !op; ns++) {
        op = lv_op_getOperator(qualName, ns);
    }
    if(!op) {
        lv_free(qualName);
        lv_cmd_message = "Error: name not found";
        return false;
    }
    char* simpleName;
    {
        size_t len = alias->len;
        simpleName = lv_alloc(len + 1);
        memcpy(simpleName, alias->start, len);
        simpleName[len] = '\0';
        char* c = simpleName;
        while((c = strchr(c, ':'))) {
            *c = '#';
        }
    }
    bool put = lv_tbl_put(&usingNames, simpleName, qualName);
    if(!put) {
        lv_free(qualName);
        lv_free(simpleName);
        lv_cmd_message = "Error: duplicate name in import";
        return false;
    }
    return true;
}

/** Adds aliases specified in the @import command */
static bool collectUsingNames(Token* nspace, Token* head) {

    if(!head || lv_tkn_cmp(head, "using") != 0) {
        //using not specified
        return true;
    }
    //using specified
    head = head->next;
    if(!head) {
        lv_cmd_message = "Error: no aliases given";
        return false;
    }
    if(lv_tkn_cmp(head, "_") == 0) {
        bool success = wildcardImport(nspace, head->next, &head);
        if(!success) {
            return false;
        }
    }
    while(head) {
        Token* original;
        Token* alias;
        bool success = getAlias(head, &original, &alias, &head);
        if(!success) {
            return false;
        }
        success = addFunctionAlias(nspace, original, alias);
        if(!success) {
            return false;
        }
        if(head) {
            //more aliases!
            if(head->start[0] != ',') {
                lv_cmd_message = "Error: unexpected token";
                return false;
            }
            head = head->next;
        }
    }
    return true;
}

static bool import(Token* head) {

    head = head->next;
    if(!head) {
        lv_cmd_message = "Usage: @import <file> [as <name>] [using ...]";
        return false;
    }
    if(head->type != TTY_IDENT) {
        lv_cmd_message = "Error: Invalid argument format";
        return false;
    }
    //save using names
    struct Scopes saveScopes = nameScopes;
    Hashtable saveUsingNames = usingNames;
    initNameScopes();
    lv_tbl_init(&usingNames);
    //import the file
    char filename[head->len + 1];
    memcpy(filename, head->start, head->len);
    filename[head->len] = '\0';
    bool res = lv_readFile(filename);
    //restore using names
    lv_tbl_clear(&usingNames, freeUsingNames);
    lv_free(usingNames.table);
    freeNameScopes();
    nameScopes = saveScopes;
    usingNames = saveUsingNames;
    if(!res) {
        lv_cmd_message = "Error: reading import";
        return false;
    }
    //collect the using names specified after the file
    res = collectUsingNames(head, head->next);
    if(res) {
        lv_cmd_message = "Import successful";
        return true;
    } else {
        //lv_cmd_message = "Import failed";
        return false;
    }
}
