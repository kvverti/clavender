#include "expression.h"
#include "lavender.h"
#include "command.h"
#include "operator.h"
#include <string.h>
#include <assert.h>

#define REQUIRE_MORE_TOKENS(x) \
    if(!(x)) { LV_EXPR_ERROR = XPE_UNTERM_EXPR; return RETVAL; } else (void)0
#define INCR_HEAD(x) { REQUIRE_MORE_TOKENS((x)->next); (x) = (x)->next; }

/**
 * Context for the declaration helper functions.
 * Not all fields may be initialized in helper functions.
 */
static struct DeclContext {
    Token* head;          //current token
    Operator* nspace;     //pointer to enclosing function
    char* name;           //function name
    int arity;            //function arity
    int locals;           //number of function locals
    Token* localStart;    //start of function local list
    Fixing fixing;        //function fixing
    Param* args;
    size_t numArgs;
    bool varargs;
} context;

static bool specifiesFixing(void);
static void parseArity(void);
static void parseLocals(Token* head); //called by parseArity
static void parseNameAndFixing(void);
static void setupArgsArray(Param params[]);

static Operator* declareFunctionImpl(Token* tok, Operator* nspace, Token** bodyTok);

Operator* lv_expr_declareFunction(Token* tok, Operator* nspace, Token** bodyTok) {

    context.name = NULL;
    context.args = NULL;
    context.numArgs = 0;
    Operator* res = declareFunctionImpl(tok, nspace, bodyTok);
    if(!res) {
        //cleanup
        for(size_t i = 0; i < context.numArgs; i++) {
            lv_free(context.args[i].name);
        }
        lv_free(context.args);
        lv_free(context.name);
    }
    return res;
}

static Operator* declareFunctionImpl(Token* tok, Operator* nspace, Token** bodyTok) {

    #define RETVAL NULL
    if(LV_EXPR_ERROR) {
        *bodyTok = tok;
        return NULL;
    }
    assert(tok);
    assert(nspace->type == FUN_FWD_DECL);
    context.head = tok;
    context.nspace = nspace;
    //skip opening paren if one is present
    if(context.head->type == TTY_LITERAL && context.head->start[0] == '(') {
        context.head = context.head->next;
    }
    if(!context.head || lv_tkn_cmp(context.head, "def") != 0) {
        //this is not a function!
        LV_EXPR_ERROR = XPE_NOT_FUNCT;
        *bodyTok = context.head;
        return NULL;
    }
    *bodyTok = context.head;
    INCR_HEAD(context.head);
    //is this a named function? If so, get fixing as well
    parseNameAndFixing();
    if(LV_EXPR_ERROR) {
        *bodyTok = context.head;
        return NULL;
    }
    if(context.head->type == TTY_LITERAL && context.head->start[0] != '(') {
        //we require a left paren before the arguments
        LV_EXPR_ERROR = XPE_EXPT_ARGS;
        *bodyTok = context.head;
        return NULL;
    }
    if(context.head->type == TTY_EMPTY_ARGS) {
        //no formal parameters, but maybe still locals
        context.arity = 0;
        *bodyTok = context.head;
        INCR_HEAD(context.head);
        parseLocals(context.head);
    } else {
        *bodyTok = context.head;
        INCR_HEAD(context.head);
        //collect args
        parseArity();
        if(LV_EXPR_ERROR) {
            *bodyTok = context.head;
            return NULL;
        }
    }
    //only prefix functions may have arity 0
    //and right infix functions may not have arity 1
    if((context.arity == 0 && context.fixing != FIX_PRE)
    || (context.arity == 1 && context.fixing == FIX_RIGHT_IN)) {
        LV_EXPR_ERROR = XPE_BAD_FIXING;
        *bodyTok = context.head;
        return NULL;
    }
    //holds the parameters (formal, captured, and local) and their names
    int totalParams = context.numArgs = context.arity + nspace->arity + nspace->locals + context.locals;
    Param* args = context.args = lv_alloc(totalParams * sizeof(Param));
    memset(args, 0, totalParams * sizeof(Param));
    //set up the args array
    setupArgsArray(args);
    if(LV_EXPR_ERROR) {
        *bodyTok = context.head;
        return NULL;
    }
    if(lv_tkn_cmp(context.head, "=>") != 0) {
        //sorry, a function body is required
        LV_EXPR_ERROR = XPE_MISSING_BODY;
        *bodyTok = context.head;
        return NULL;
    }
    //the head is now at the arrow token
    //the body starts with the token after the =>
    //it must exist, sorry
    *bodyTok = context.head;
    INCR_HEAD(context.head);
    //build the function name
    // buildFuncName();
    FuncNamespace ns = context.fixing == FIX_PRE ? FNS_PREFIX : FNS_INFIX;
    //check to see if this function was defined twice
    Operator* funcObj = lv_op_getOperator(context.name, ns);
    if(funcObj) {
        //let's disallow entirely
        LV_EXPR_ERROR = XPE_DUP_DECL;
        *bodyTok = tok;
        return NULL;
    } else {
        //add a new one
        funcObj = lv_alloc(sizeof(Operator));
        funcObj->name = context.name;
        funcObj->next = NULL;
        funcObj->type = FUN_FWD_DECL;
        funcObj->arity = totalParams - context.locals;
        funcObj->fixing = context.fixing;
        funcObj->captureCount = nspace->arity + nspace->locals;
        funcObj->locals = context.locals;
        funcObj->params = args;
        funcObj->varargs = context.varargs;
        funcObj->enclosing = nspace;
        lv_op_addOperator(funcObj, ns);
        *bodyTok = context.head;
        return funcObj;
    }
    #undef RETVAL
}

//helpers for lv_expr_declareFunction

static void parseNameAndFixing(void) {

    #define RETVAL
    char nul = '\0';
    char* name;
    size_t nameLen;
    switch(context.head->type) {
        case TTY_IDENT:
        case TTY_FUNC_SYMBOL:
        case TTY_SYMBOL:
            //save the name
            //check fixing
            if(specifiesFixing()) {
                context.fixing = context.head->start[0];
                name = context.head->start + 2;
                nameLen = context.head->len - 2;
            } else {
                context.fixing = FIX_PRE;
                name = context.head->start;
                nameLen = context.head->len;
            }
            if(lv_expr_isReserved(name, nameLen)) {
                LV_EXPR_ERROR = XPE_RESERVED_ID;
                return;
            }
            INCR_HEAD(context.head);
            break;
        default:
            context.fixing = FIX_PRE;
            name = &nul;
            nameLen = 0;
    }
    //slap the namespace name on it
    //plus 1 for the colon
    int nsOffset = strlen(context.nspace->name) + 1;
    //plus 1 for the colon, plus 1 for the NUL terminator
    char* fqn = lv_alloc(strlen(context.nspace->name) + nameLen + 2);
    strncpy(fqn + nsOffset, name, nameLen);
    fqn[nsOffset + nameLen] = '\0';
    //change ':' in symbolic name to '#'
    //because namespaces use ':' as a separator
    char* colon = fqn + nsOffset;
    while((colon = strchr(colon, ':')))
        *colon = '#';
    //add namespace
    strcpy(fqn, context.nspace->name);
    fqn[nsOffset - 1] = ':';
    context.name = fqn;
    #undef RETVAL
}

/**
 * Puts information (name and passing convention) on each
 * function parameter. Function parameters include (in this order):
 * formal parameters, all parameters from the enclosing function,
 * function local parameters.
 */
static void setupArgsArray(Param params[]) {

    #define RETVAL
    //context.arity is the number of formal parameters
    int arity = context.arity;
    //assign formal parameters
    for(int i = 0; i < arity; i++) {
        params[i].initializer = NULL;
        params[i].byName = (context.head->type == TTY_SYMBOL);
        if(params[i].byName) //incr past by name symbol
            context.head = context.head->next;
        if(context.head->type == TTY_ELLIPSIS) { //incr past varargs
            if(context.fixing != FIX_PRE && arity == 1) {
                //cannot have a postfix varargs
                LV_EXPR_ERROR = XPE_BAD_ARGS;
                return;
            }
            context.head = context.head->next;
        }
        assert(context.head->type == TTY_IDENT);
        params[i].name = lv_alloc(context.head->len + 1);
        memcpy(params[i].name, context.head->start, context.head->len);
        params[i].name[context.head->len] = '\0';
        assert(context.head->next->type == TTY_LITERAL);
        context.head = context.head->next->next; //skip comma or close paren
    }
    //copy over captured params (if any)
    memcpy(params + arity, context.nspace->params,
        (context.nspace->arity + context.nspace->locals) * sizeof(Param));
    int offset = arity + context.nspace->arity + context.nspace->locals;
    for(size_t i = arity; i < offset; i++) {
        //copy param names
        Param* param = &context.nspace->params[i - arity];
        size_t len = strlen(param->name);
        params[i].name = lv_alloc(len + 1);
        memcpy(params[i].name, param->name, len);
        params[i].name[len] = '\0';
    }
    //assign function locals
    Token* currentLocal = context.localStart;
    for(int i = offset; i < (offset + context.locals); i++) {
        //locals are never by name
        params[i].byName = false;
        //get local name from currentLocal
        assert(currentLocal->type == TTY_IDENT);
        params[i].name = lv_alloc(currentLocal->len + 1);
        memcpy(params[i].name, currentLocal->start, currentLocal->len);
        params[i].name[currentLocal->len] = '\0';
        currentLocal = currentLocal->next;
        //consume open paren
        assert(currentLocal->start[0] == '(');
        currentLocal = currentLocal->next;
        //keep track of nesting so we know when to stop
        int parenNesting = 0;
        //store the beginning of the init expression
        params[i].initializer = currentLocal;
        do {
            //check type because empty args exists
            if(currentLocal->type == TTY_LITERAL) {
                switch(currentLocal->start[0]) {
                    case '(': parenNesting++; break;
                    case ')': parenNesting--; break;
                }
            }
            currentLocal = currentLocal->next;
        } while(parenNesting >= 0);
        //consume the comma
        if(currentLocal->start[0] == ',') {
            INCR_HEAD(currentLocal);
        }
        context.head = currentLocal;
    }
    #undef RETVAL
}

static bool specifiesFixing(void) {

    switch(context.head->type) {
        case TTY_FUNC_SYMBOL:
            return true;
        case TTY_IDENT: {
            char c = context.head->start[0];
            return (c == 'i' || c == 'r' || c == 'u')
                && context.head->len > 2
                && context.head->start[1] == '_';
        }
        default:
            return false;
    }
}

//gets the arity of the function
//also validates the argument list
static void parseArity(void) {

    #define RETVAL
    Token* head = context.head;
    assert(head);
    if(head->start[0] == ')') {
        INCR_HEAD(head);
        //important to set before parseLocals
        //because parseLocals sets context.head if it fails
        context.head = head;
        parseLocals(head);
        context.arity = 0;
        context.varargs = false;
        return;
    }
    int res = 0;
    bool varargs = false;
    while(true) {
        if(varargs) {
            //varargs only allowed at the end
            LV_EXPR_ERROR = XPE_BAD_ARGS;
            return;
        }
        //by name symbol?
        if(lv_tkn_cmp(head, "=>") == 0) {
            //by name symbol
            INCR_HEAD(head);
        }
        if(head->type == TTY_ELLIPSIS) {
            //varargs modifier
            varargs = true;
            INCR_HEAD(head);
        }
        //is it a param name
        if(head->type == TTY_IDENT) {
            if(lv_expr_isReserved(head->start, head->len)) {
                LV_EXPR_ERROR = XPE_RESERVED_ID;
                return;
            }
            res++;
            INCR_HEAD(head);
            //must be a comma or a close paren
            if(head->start[0] == ')') {
                //incr past close paren
                INCR_HEAD(head);
                break; //we're done
            } else if(head->start[0] != ',') {
                //must separate params with commas!
                LV_EXPR_ERROR = XPE_BAD_ARGS;
                return;
            } else {
                //it's a comma, increment
                INCR_HEAD(head);
            }
        } else {
            //malformed argument list
            LV_EXPR_ERROR = XPE_BAD_ARGS;
            return;
        }
    }
    parseLocals(head);
    context.arity = res;
    context.varargs = varargs;
    #undef RETVAL
}

/**
 * A word on locals:
 * This function validates that the function local values are
 * properly formatted, counts the number of function locals, and saves
 * a pointer to the first function local declaration. The function
 * locals' initializers are actually parsed when the function is defined
 * (this file handles declaring functions). Note that we duplicate the walking
 * (minus the validation) later in setupArgsArray().
 */
static void parseLocals(Token* head) {

    #define RETVAL
    if(lv_tkn_cmp(head, "let") == 0) {
        //this function has defines function locals
        //the locals are of the form <id>(<expr>) , ...
        //and end when we reach the arrow token
        int locals = 0;
        context.localStart = head->next;
        do {
            locals++;
            INCR_HEAD(head);
            //check that this is a name
            if(head->type != TTY_IDENT) {
                LV_EXPR_ERROR = XPE_BAD_LOCALS;
                context.head = head;
                return;
            }
            //that is not reserved
            if(lv_expr_isReserved(head->start, head->len)) {
                LV_EXPR_ERROR = XPE_RESERVED_ID;
                context.head = head;
                return;
            }
            INCR_HEAD(head);
            //check for the opening paren
            if(head->start[0] != '(') {
                LV_EXPR_ERROR = XPE_UNEXPECT_TOKEN;
                context.head = head;
                return;
            }
            INCR_HEAD(head);
            //we must remember the number of parens inside the expression
            int numParens = 0;
            do {
                //check type because empty args exists
                if(head->type == TTY_LITERAL) {
                    switch(head->start[0]) {
                        case '(': numParens++; break;
                        case ')': numParens--; break;
                    }
                }
                INCR_HEAD(head);
            } while(numParens >= 0);
            //head should point at a comma or arrow
        } while(head->start[0] == ',');
        context.locals = locals;
    } else {
        context.localStart = NULL;
        context.locals = 0;
    }
    #undef RETVAL
}

// static void buildFuncName(void) {
//
//     //plus 1 for the colon
//     int nsOffset = strlen(context.nspace->name) + 1;
//     //plus 1 for the colon, plus 1 for the NUL terminator
//     char* fqn = lv_alloc(strlen(context.nspace->name) + strlen(context.name) + 2);
//     strcpy(fqn + nsOffset, context.name);
//     //change ':' in symbolic name to '#'
//     //because namespaces use ':' as a separator
//     char* colon = fqn + nsOffset;
//     while((colon = strchr(colon, ':')))
//         *colon = '#';
//     //add namespace
//     strcpy(fqn, context.nspace->name);
//     fqn[nsOffset - 1] = ':';
//     context.name = fqn;
// }

#undef INCR_HEAD
#undef REQUIRE_MORE_TOKENS
