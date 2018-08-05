#include "expression.h"
#include "lavender.h"
#include "command.h"
#include "operator.h"
#include <string.h>
#include <assert.h>

#define REQUIRE_MORE_TOKENS(x) \
    if(!(x)) { LV_EXPR_ERROR = XPE_UNTERM_EXPR; return RETVAL; } else (void)0

/**
 * Context for the declaration helper functions.
 * Not all fields may be initialized in helper functions.
 */
typedef struct DeclContext {
    Token* head;          //current token
    Operator* nspace;     //pointer to enclosing function
    char* name;           //function name alias (not separately allocated!)
    Param* params;        //function parameter data
    int arity;            //function arity
    Fixing fixing;        //function fixing
    bool varargs;
} DeclContext;

static bool specifiesFixing(Token* head);
static int getArity(Token* head);
static void getNameAndFixing(DeclContext* cxt);
static void setupArgsArray(DeclContext* cxt);
static void processLocals(DeclContext* cxt);
static FuncNamespace buildFuncName(DeclContext* cxt);

Operator* lv_expr_declareFunction(Token* tok, Operator* nspace, Token** bodyTok) {

    #define RETVAL NULL
    if(LV_EXPR_ERROR)
        return NULL;
    assert(tok);
    assert(nspace->type == FUN_FWD_DECL);
    DeclContext cxt;
    cxt.head = tok;
    cxt.nspace = nspace;
    //skip opening paren if one is present
    if(cxt.head->type == TTY_LITERAL && cxt.head->value[0] == '(') {
        cxt.head = cxt.head->next;
    }
    if(!cxt.head || strcmp(cxt.head->value, "def") != 0) {
        //this is not a function!
        LV_EXPR_ERROR = XPE_NOT_FUNCT;
        return NULL;
    }
    cxt.head = cxt.head->next;
    REQUIRE_MORE_TOKENS(cxt.head);
    //is this a named function? If so, get fixing as well
    getNameAndFixing(&cxt);
    if(LV_EXPR_ERROR)
        return NULL;
    if(cxt.head->type == TTY_LITERAL && cxt.head->value[0] != '(') {
        //we require a left paren before the arguments
        LV_EXPR_ERROR = XPE_EXPT_ARGS;
        return NULL;
    }
    if(cxt.head->type == TTY_EMPTY_ARGS) {
        cxt.arity = 0;
    } else {
        cxt.head = cxt.head->next;
        REQUIRE_MORE_TOKENS(cxt.head);
        //collect args
        cxt.arity = getArity(cxt.head);
        if(LV_EXPR_ERROR)
            return NULL;
    }
    //only prefix functions may have arity 0
    //and right infix functions may not have arity 1
    if((cxt.arity == 0 && cxt.fixing != FIX_PRE)
    || (cxt.arity == 1 && cxt.fixing == FIX_RIGHT_IN)) {
        LV_EXPR_ERROR = XPE_BAD_FIXING;
        return NULL;
    }
    //holds the arguments and their names
    Param args[cxt.arity + nspace->arity];
    cxt.params = args;
    if(cxt.arity == 0) {
        //incr past close paren
        cxt.head = cxt.head->next;
        cxt.varargs = false;
    } else {
        //set up the args array
        setupArgsArray(&cxt);
        if(LV_EXPR_ERROR)
            return NULL;
    }
    REQUIRE_MORE_TOKENS(cxt.head);
    processLocals(&cxt);
    if(LV_EXPR_ERROR)
        return NULL;
    if(strcmp(cxt.head->value, "=>") != 0) {
        //sorry, a function body is required
        LV_EXPR_ERROR = XPE_MISSING_BODY;
        return NULL;
    }
    //the head is now at the arrow token
    //the body starts with the token after the =>
    //it must exist, sorry
    cxt.head = cxt.head->next;
    REQUIRE_MORE_TOKENS(cxt.head);
    //build the function name
    FuncNamespace ns = buildFuncName(&cxt);
    //check to see if this function was defined twice
    Operator* funcObj = lv_op_getOperator(cxt.name, ns);
    if(funcObj) {
        //let's disallow entirely
        LV_EXPR_ERROR = XPE_DUP_DECL;
        lv_free(cxt.name);
        return NULL;
    } else {
        //add a new one
        funcObj = lv_alloc(sizeof(Operator));
        funcObj->name = cxt.name;
        funcObj->next = NULL;
        funcObj->type = FUN_FWD_DECL;
        funcObj->arity = cxt.arity + nspace->arity;
        funcObj->fixing = cxt.fixing;
        funcObj->captureCount = nspace->arity;
        funcObj->params = lv_alloc((cxt.arity + nspace->arity) * sizeof(Param));
        funcObj->varargs = cxt.varargs;
        memcpy(funcObj->params, cxt.params, (cxt.arity + nspace->arity) * sizeof(Param));
        //copy param names
        for(int i = 0; i < (cxt.arity + nspace->arity); i++) {
            char* name = lv_alloc(strlen(cxt.params[i].name) + 1);
            strcpy(name, cxt.params[i].name);
            funcObj->params[i].name = name;
        }
        lv_op_addOperator(funcObj, ns);
        *bodyTok = cxt.head;
        return funcObj;
    }
    #undef RETVAL
}

//helpers for lv_expr_declareFunction

static void getNameAndFixing(DeclContext* cxt) {

    #define RETVAL
    switch(cxt->head->type) {
        case TTY_IDENT:
        case TTY_FUNC_SYMBOL:
        case TTY_SYMBOL:
            //save the name
            //check fixing
            if(specifiesFixing(cxt->head)) {
                cxt->fixing = cxt->head->value[0];
                cxt->name = cxt->head->value + 2;
            } else {
                cxt->fixing = FIX_PRE;
                cxt->name = cxt->head->value;
            }
            cxt->head = cxt->head->next;
            REQUIRE_MORE_TOKENS(cxt->head);
            break;
        default:
            cxt->fixing = FIX_PRE;
            cxt->name = "";
    }
    #undef RETVAL
}

static void setupArgsArray(DeclContext* cxt) {

    int arity = cxt->arity;
    cxt->varargs = false;
    for(int i = 0; i < arity; i++) {
        cxt->params[i].byName = (cxt->head->type == TTY_SYMBOL);
        if(cxt->params[i].byName) //incr past by name symbol
            cxt->head = cxt->head->next;
        if(cxt->head->type == TTY_ELLIPSIS) { //incr past varargs
            if(cxt->fixing != FIX_PRE && arity == 1) {
                //cannot have a postfix varargs
                LV_EXPR_ERROR = XPE_BAD_ARGS;
                return;
            }
            cxt->varargs = true;
            cxt->head = cxt->head->next;
        }
        assert(cxt->head->type == TTY_IDENT);
        cxt->params[i].name = cxt->head->value;
        assert(cxt->head->next->type == TTY_LITERAL);
        cxt->head = cxt->head->next->next; //skip comma or close paren
    }
    //copy over captured params (if any)
    memcpy(cxt->params + arity, cxt->nspace->params, cxt->nspace->arity * sizeof(Param));
}

static bool specifiesFixing(Token* head) {

    switch(head->type) {
        case TTY_FUNC_SYMBOL:
            return true;
        case TTY_IDENT: {
            char c = head->value[0];
            return (c == 'i' || c == 'r' || c == 'u')
                && head->value[1] == '_'
                && head->value[2] != '\0';
        }
        default:
            return false;
    }
}

//gets the arity of the function
//also validates the argument list
static int getArity(Token* head) {

    #define RETVAL 0
    assert(head);
    int res = 0;
    bool varargs = false;
    if(head->value[0] == ')')
        return res;
    while(true) {
        if(varargs) {
            //varargs only allowed at the end
            LV_EXPR_ERROR = XPE_BAD_ARGS;
            return 0;
        }
        //by name symbol?
        if(strcmp(head->value, "=>") == 0) {
            //by name symbol
            head = head->next;
            REQUIRE_MORE_TOKENS(head);
        }
        if(head->type == TTY_ELLIPSIS) {
            //varargs modifier
            varargs = true;
            head = head->next;
            REQUIRE_MORE_TOKENS(head);
        }
        //is it a param name
        if(head->type == TTY_IDENT) {
            res++;
            head = head->next;
            REQUIRE_MORE_TOKENS(head);
            //must be a comma or a close paren
            if(head->value[0] == ')')
                break; //we're done
            else if(head->value[0] != ',') {
                //must separate params with commas!
                LV_EXPR_ERROR = XPE_BAD_ARGS;
                return 0;
            } else {
                //it's a comma, increment
                head = head->next;
                REQUIRE_MORE_TOKENS(head);
            }
        } else {
            //malformed argument list
            LV_EXPR_ERROR = XPE_BAD_ARGS;
            return 0;
        }
    }
    return res;
    #undef RETVAL
}

static void processLocals(DeclContext* cxt) {

    #define RETVAL
    if(strcmp(cxt->head->value, "let") == 0) {
        //this function has defines function locals
        //the locals are of the form <id>(<expr>) , ...
        //and end when we reach the arrow token
        do {
            cxt->head = cxt->head->next;
            REQUIRE_MORE_TOKENS(cxt->head);
            //check that this is a name
            if(cxt->head->type != TTY_IDENT) {
                LV_EXPR_ERROR = XPE_BAD_LOCALS;
                return;
            }
            //TODO: save the local names as fake params
            cxt->head = cxt->head->next;
            REQUIRE_MORE_TOKENS(cxt->head);
            //check for the opening paren
            if(cxt->head->value[0] != '(') {
                LV_EXPR_ERROR = XPE_UNEXPECT_TOKEN;
                return;
            }
            cxt->head = cxt->head->next;
            REQUIRE_MORE_TOKENS(cxt->head);
            //we must remember the number of parens inside the expression
            int numParens = 0;
            do {
                switch(cxt->head->value[0]) {
                    case '(': numParens++; break;
                    case ')': numParens--; break;
                }
                cxt->head = cxt->head->next;
                REQUIRE_MORE_TOKENS(cxt->head);
            } while(numParens >= 0);
            //head should point at a comma or arrow
        } while(cxt->head->value[0] == ',');
    }
    #undef RETVAL
}

static FuncNamespace buildFuncName(DeclContext* cxt) {

    FuncNamespace ns = cxt->fixing == FIX_PRE ? FNS_PREFIX : FNS_INFIX;
    int nsOffset = strlen(cxt->nspace->name) + 1;
    char* fqn = lv_alloc(strlen(cxt->nspace->name) + strlen(cxt->name) + 2);
    strcpy(fqn + nsOffset, cxt->name);
    //change ':' in symbolic name to '#'
    //because namespaces use ':' as a separator
    char* colon = fqn + nsOffset;
    while((colon = strchr(colon, ':')))
        *colon = '#';
    //add namespace
    strcpy(fqn, cxt->nspace->name);
    fqn[nsOffset - 1] = ':';
    cxt->name = fqn;
    return ns;
}

#undef REQUIRE_MORE_TOKENS
