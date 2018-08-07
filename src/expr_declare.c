#include "expression.h"
#include "lavender.h"
#include "command.h"
#include "operator.h"
#include <string.h>
#include <assert.h>

#define REQUIRE_MORE_TOKENS(x) \
    if(!(x)) { LV_EXPR_ERROR = XPE_UNTERM_EXPR; return RETVAL; } else (void)0
#define INCR_HEAD(x) { (x) = (x)->next; REQUIRE_MORE_TOKENS(x); }

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
    bool varargs;
} context;

static bool specifiesFixing(void);
static void parseArity(void);
static void parseLocals(void);
static void parseNameAndFixing(void);
static void setupArgsArray(Param params[]);
static void buildFuncName(void);

Operator* lv_expr_declareFunction(Token* tok, Operator* nspace, Token** bodyTok) {

    #define RETVAL NULL
    if(LV_EXPR_ERROR)
        return NULL;
    assert(tok);
    assert(nspace->type == FUN_FWD_DECL);
    context.head = tok;
    context.nspace = nspace;
    //skip opening paren if one is present
    if(context.head->type == TTY_LITERAL && context.head->value[0] == '(') {
        context.head = context.head->next;
    }
    if(!context.head || strcmp(context.head->value, "def") != 0) {
        //this is not a function!
        LV_EXPR_ERROR = XPE_NOT_FUNCT;
        return NULL;
    }
    INCR_HEAD(context.head);
    //is this a named function? If so, get fixing as well
    parseNameAndFixing();
    if(LV_EXPR_ERROR)
        return NULL;
    if(context.head->type == TTY_LITERAL && context.head->value[0] != '(') {
        //we require a left paren before the arguments
        LV_EXPR_ERROR = XPE_EXPT_ARGS;
        return NULL;
    }
    if(context.head->type == TTY_EMPTY_ARGS) {
        context.arity = 0;
    } else {
        INCR_HEAD(context.head);
        //collect args
        parseArity();
        if(LV_EXPR_ERROR)
            return NULL;
    }
    //only prefix functions may have arity 0
    //and right infix functions may not have arity 1
    if((context.arity == 0 && context.fixing != FIX_PRE)
    || (context.arity == 1 && context.fixing == FIX_RIGHT_IN)) {
        LV_EXPR_ERROR = XPE_BAD_FIXING;
        return NULL;
    }
    //holds the arguments and their names
    Param args[context.arity + nspace->arity];
    if(context.arity == 0) {
        //incr past close paren
        context.head = context.head->next;
    } else {
        //set up the args array
        setupArgsArray(args);
        if(LV_EXPR_ERROR)
            return NULL;
    }
    REQUIRE_MORE_TOKENS(context.head);
    parseLocals();
    if(strcmp(context.head->value, "=>") != 0) {
        //sorry, a function body is required
        LV_EXPR_ERROR = XPE_MISSING_BODY;
        return NULL;
    }
    //the head is now at the arrow token
    //the body starts with the token after the =>
    //it must exist, sorry
    INCR_HEAD(context.head);
    //build the function name
    buildFuncName();
    FuncNamespace ns = context.fixing == FIX_PRE ? FNS_PREFIX : FNS_INFIX;
    //check to see if this function was defined twice
    Operator* funcObj = lv_op_getOperator(context.name, ns);
    if(funcObj) {
        //let's disallow entirely
        LV_EXPR_ERROR = XPE_DUP_DECL;
        lv_free(context.name);
        return NULL;
    } else {
        //add a new one
        funcObj = lv_alloc(sizeof(Operator));
        funcObj->name = context.name;
        funcObj->next = NULL;
        funcObj->type = FUN_FWD_DECL;
        funcObj->arity = context.arity + nspace->arity;
        funcObj->fixing = context.fixing;
        funcObj->captureCount = nspace->arity;
        funcObj->locals = context.locals;
        funcObj->params = lv_alloc((context.arity + nspace->arity) * sizeof(Param));
        funcObj->varargs = context.varargs;
        memcpy(funcObj->params, args, (context.arity + nspace->arity) * sizeof(Param));
        //copy param names
        for(int i = 0; i < (context.arity + nspace->arity); i++) {
            char* name = lv_alloc(strlen(args[i].name) + 1);
            strcpy(name, args[i].name);
            funcObj->params[i].name = name;
        }
        lv_op_addOperator(funcObj, ns);
        *bodyTok = context.head;
        return funcObj;
    }
    #undef RETVAL
}

//helpers for lv_expr_declareFunction

static void parseNameAndFixing(void) {

    #define RETVAL
    switch(context.head->type) {
        case TTY_IDENT:
        case TTY_FUNC_SYMBOL:
        case TTY_SYMBOL:
            //save the name
            //check fixing
            if(specifiesFixing()) {
                context.fixing = context.head->value[0];
                context.name = context.head->value + 2;
            } else {
                context.fixing = FIX_PRE;
                context.name = context.head->value;
            }
            INCR_HEAD(context.head);
            break;
        default:
            context.fixing = FIX_PRE;
            context.name = "";
    }
    #undef RETVAL
}

static void setupArgsArray(Param params[]) {

    int arity = context.arity;
    //assign formal parameters
    for(int i = 0; i < arity; i++) {
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
        params[i].name = context.head->value;
        assert(context.head->next->type == TTY_LITERAL);
        context.head = context.head->next->next; //skip comma or close paren
    }
    //copy over captured params (if any)
    memcpy(params + arity, context.nspace->params, context.nspace->arity * sizeof(Param));
}

static bool specifiesFixing(void) {

    switch(context.head->type) {
        case TTY_FUNC_SYMBOL:
            return true;
        case TTY_IDENT: {
            char c = context.head->value[0];
            return (c == 'i' || c == 'r' || c == 'u')
                && context.head->value[1] == '_'
                && context.head->value[2] != '\0';
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
    if(head->value[0] == ')') {
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
        if(strcmp(head->value, "=>") == 0) {
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
            res++;
            INCR_HEAD(head);
            //must be a comma or a close paren
            if(head->value[0] == ')')
                break; //we're done
            else if(head->value[0] != ',') {
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
    context.arity = res;
    context.varargs = varargs;
    #undef RETVAL
}

/**
 * A word on locals:
 * This function simply validates that the function local values are
 * properly formatted, counts the number of function locals, and saves
 * a pointer to the first function local declaration. The function locals'
 * names and initializers are actually parsed when the function is defined
 * (this file handles declaring functions). This is not a problem because
 * function locals may only be used after their initialization.
 */
static void parseLocals(void) {

    //TODO: extract the local names as well using a DynBuffer
    #define RETVAL
    if(strcmp(context.head->value, "let") == 0) {
        //this function has defines function locals
        //the locals are of the form <id>(<expr>) , ...
        //and end when we reach the arrow token
        int locals = 0;
        context.localStart = context.head->next;
        do {
            locals++;
            INCR_HEAD(context.head);
            //check that this is a name
            if(context.head->type != TTY_IDENT) {
                LV_EXPR_ERROR = XPE_BAD_LOCALS;
                return;
            }
            INCR_HEAD(context.head);
            //check for the opening paren
            if(context.head->value[0] != '(') {
                LV_EXPR_ERROR = XPE_UNEXPECT_TOKEN;
                return;
            }
            INCR_HEAD(context.head);
            //we must remember the number of parens inside the expression
            int numParens = 0;
            do {
                switch(context.head->value[0]) {
                    case '(': numParens++; break;
                    case ')': numParens--; break;
                }
                INCR_HEAD(context.head);
            } while(numParens >= 0);
            //head should point at a comma or arrow
        } while(context.head->value[0] == ',');
        context.locals = locals;
    } else {
        context.localStart = NULL;
        context.locals = 0;
    }
    #undef RETVAL
}

static void buildFuncName(void) {

    //plus 1 for the colon
    int nsOffset = strlen(context.nspace->name) + 1;
    //plus 1 for the colon, plus 1 for the NUL terminator
    char* fqn = lv_alloc(strlen(context.nspace->name) + strlen(context.name) + 2);
    strcpy(fqn + nsOffset, context.name);
    //change ':' in symbolic name to '#'
    //because namespaces use ':' as a separator
    char* colon = fqn + nsOffset;
    while((colon = strchr(colon, ':')))
        *colon = '#';
    //add namespace
    strcpy(fqn, context.nspace->name);
    fqn[nsOffset - 1] = ':';
    context.name = fqn;
}

#undef INCR_HEAD
#undef REQUIRE_MORE_TOKENS
