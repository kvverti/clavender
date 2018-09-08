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
// static bool using(Token* head);

static CommandElement COMMANDS[] = {
    { "quit", quit },
    { "import", import },
    // { "using", using },
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

//end hashtable impl

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

// static bool using(Token* head) {
//
//     head = head->next;
//     if(!head || head->next) {
//         lv_cmd_message = "Usage: @using <name(space)>";
//         return false;
//     }
//     switch(head->type) {
//         case TTY_IDENT: {
//             //using namespace
//             char* scope;
//             {
//                 size_t len = strlen(head->value) + 1;
//                 scope = lv_alloc(len);
//                 memcpy(scope, head->value, len);
//             }
//             addScope(scope);
//             lv_cmd_message = "Using successful";
//             return true;
//         }
//         case TTY_QUAL_IDENT:
//         case TTY_QUAL_SYMBOL: {
//             //using specific
//             Operator* op = NULL;
//             for(FuncNamespace ns = 0; (ns < FNS_COUNT) && !op; ns++) {
//                 op = lv_op_getOperator(head->value, ns);
//             }
//             if(!op) {
//                 lv_cmd_message = "Error: name not found";
//                 return false;
//             }
//             char* qualName;
//             {
//                 size_t len = strlen(head->value) + 1;
//                 qualName = lv_alloc(len);
//                 memcpy(qualName, head->value, len);
//             }
//             char* simpleName;
//             {
//                 char* tmp = strchr(qualName, ':') + 1;
//                 size_t len = strlen(tmp) + 1;
//                 simpleName = lv_alloc(len);
//                 memcpy(simpleName, tmp, len);
//             }
//             bool put = lv_tbl_put(&usingNames, simpleName, qualName);
//             if(!put) {
//                 lv_free(qualName);
//                 lv_free(simpleName);
//             }
//             lv_cmd_message = "Using successful";
//             return true;
//         }
//         default:
//             lv_cmd_message = "Error: not a valid name";
//             return false;
//     }
//     assert(false);
// }

static inline bool isIdent(TokenType type) {

    return type == TTY_IDENT || type == TTY_SYMBOL;
}

/**
 * Gets the alias from head, returning it, and storing the next
 * unread token in end. Returns false if a problem occurred.
 */
static bool getAlias(Token* head, char** original, char** alias, Token** end) {

    //pieces in the form of <id> [as <alias>]
    assert(end);
    if(isIdent(head->type)) {
        //declared id is the default alias
        *original = head->value;
        *alias = head->value;
        head = head->next;
        if(head) {
            if(strcmp(head->value, "as") == 0) {
                head = head->next;
                //this token had better be an alias
                if(!head || !isIdent(head->type)) {
                    lv_cmd_message = "Error: expected alias";
                    return false;
                }
                //new alias specified
                *alias = head->value;
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

static bool wildcardImport(char* nspace, Token* head, Token** end) {
    //using everything!
    char* scope;
    {
        size_t len = strlen(nspace) + 1;
        scope = lv_alloc(len);
        memcpy(scope, nspace, len);
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
static bool addFunctionAlias(char* nspace, char* original, char* alias) {

    char* qualName;
    {
        size_t alen = strlen(nspace);
        size_t blen = strlen(original);
        qualName = lv_alloc(alen + 1 + blen + 1);
        memcpy(qualName, nspace, alen);
        qualName[alen] = ':';
        memcpy(qualName + alen + 1, original, blen);
        qualName[alen + 1 + blen] = '\0';
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
        size_t len = strlen(alias) + 1;
        simpleName = lv_alloc(len);
        memcpy(simpleName, alias, len);
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
static bool collectUsingNames(char* nspace, Token* head) {

    if(!head || strcmp(head->value, "using") != 0) {
        //using not specified
        return true;
    }
    //using specified
    head = head->next;
    if(!head) {
        lv_cmd_message = "Error: no aliases given";
        return false;
    }
    if(strcmp(head->value, "_") == 0) {
        bool success = wildcardImport(nspace, head->next, &head);
        if(!success) {
            return false;
        }
    }
    while(head) {
        char* original;
        char* alias;
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
            if(head->value[0] != ',') {
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
    initNameScopes();
    //import the file
    bool res = lv_readFile(head->value);
    //restore using names
    freeNameScopes();
    nameScopes = saveScopes;
    if(!res) {
        lv_cmd_message = "Error: reading import";
        return false;
    }
    //collect the using names specified after the file
    res = collectUsingNames(head->value, head->next);
    if(res) {
        lv_cmd_message = "Import successful";
        return true;
    } else {
        //lv_cmd_message = "Import failed";
        return false;
    }
}
