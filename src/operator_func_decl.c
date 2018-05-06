#include "operator.h"
#include "lavender.h"
#include <string.h>
#include <assert.h>

#define REQUIRE_MORE_TOKENS(x) \
    if(!(x)) { LV_OP_ERROR = OPE_UNTERM_EXPR; return RETVAL; } else (void)0

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
    if(head->value[0] == ')')
        return res;
    while(true) {
        //by name symbol?
        if(strcmp(head->value, "=>") == 0) {
            //by name symbol
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
                LV_OP_ERROR = OPE_BAD_ARGS;
                return 0;
            } else {
                //it's a comma, increment
                head = head->next;
                REQUIRE_MORE_TOKENS(head);
            }
        } else {
            //malformed argument list
            LV_OP_ERROR = OPE_BAD_ARGS;
            return 0;
        }
    }
    return res;
    #undef RETVAL
}

Operator* lv_op_declareFunction(Token* head, char* nspace, Token** bodyTok) {
    
    #define RETVAL NULL
    if(LV_OP_ERROR)
        return NULL;
    assert(head);
    char* fnameAlias;       //function name alias (not separately allocated!)
    int arity;              //function arity
    Fixing fixing;          //function fixing
    //how many levels of parens nested we are
    //we stop parsing when this becomes less than zero
    int nesting = 0;
    //skip opening paren if one is present
    if(head->value[0] == '(') {
        nesting++;
        head = head->next;
    }
    if(!head || strcmp(head->value, "def") != 0) {
        //this is not a function!
        LV_OP_ERROR = OPE_NOT_FUNCT;
        return NULL;
    }
    head = head->next;
    REQUIRE_MORE_TOKENS(head);
    //is this a named function? If so, get fixing as well
    switch(head->type) {
        case TTY_IDENT:
        case TTY_FUNC_SYMBOL:
        case TTY_SYMBOL:
            //save the name
            //check fixing
            if(specifiesFixing(head)) {
                fixing = head->value[0];
                fnameAlias = head->value + 2;
            } else {
                fixing = FIX_PRE;
                fnameAlias = head->value;
            }
            head = head->next;
            REQUIRE_MORE_TOKENS(head);
            break;
        default:
            fixing = FIX_PRE;
            fnameAlias = "";
    }
    if(head->value[0] != '(') {
        //we require a left paren before the arguments
        LV_OP_ERROR = OPE_EXPT_ARGS;
        return NULL;
    }
    head = head->next;
    REQUIRE_MORE_TOKENS(head);
    //collect args
    arity = getArity(head);
    if(LV_OP_ERROR)
        return NULL;
    //holds the arguments and their names
    Param args[arity];
    if(arity == 0) {
        //incr past close paren
        head = head->next;
    } else {
        //set up the args array
        for(int i = 0; i < arity; i++) {
            args[i].byName = (head->type == TTY_SYMBOL);
            if(args[i].byName) //incr past by name symbol
                head = head->next;
            assert(head->type == TTY_IDENT);
            args[i].name = head->value;
            assert(head->next->type == TTY_LITERAL);
            head = head->next->next; //skip comma or close paren
        }
    }
    REQUIRE_MORE_TOKENS(head);
    if(strcmp(head->value, "=>") != 0) {
        //sorry, a function body is required
        LV_OP_ERROR = OPE_MISSING_BODY;
        return NULL;
    }
    //the body starts with the token after the =>
    //it must exist, sorry
    head = head->next;
    REQUIRE_MORE_TOKENS(head);
    //build the function name
    FuncNamespace ns = fixing == FIX_PRE ? FNS_PREFIX : FNS_INFIX;
    int nsOffset = strlen(nspace) + 1;
    char* fqn = lv_alloc(strlen(nspace) + strlen(fnameAlias) + 2);
    strcpy(fqn + nsOffset, fnameAlias);
    //change ':' in symbolic name to '#'
    //because namespaces use ':' as a separator
    char* colon = fqn + nsOffset;
    while((colon = strchr(colon, ':')))
        *colon = '#';
    //add namespace
    strcpy(fqn, nspace);
    fqn[nsOffset - 1] = ':';
    //check to see if this function was defined twice
    Operator* funcObj = lv_op_getOperator(fqn, ns);
    if(funcObj) {
        //let's disallow entirely
        LV_OP_ERROR = OPE_DUP_DECL;
        lv_free(fqn);
        return NULL;
    } else {
        //add a new one
        funcObj = lv_alloc(sizeof(Operator));
        funcObj->name = fqn;
        funcObj->next = NULL;
        funcObj->type = FUN_FWD_DECL;
        funcObj->arity = arity;
        funcObj->fixing = fixing;
        funcObj->captureCount = 0; //todo
        funcObj->params = lv_alloc(arity * sizeof(Param));
        memcpy(funcObj->params, args, arity * sizeof(Param));
        //copy param names
        for(int i = 0; i < arity; i++) {
            char* name = lv_alloc(strlen(args[i].name) + 1);
            strcpy(name, args[i].name);
            funcObj->params[i].name = name;
        }
        lv_op_addOperator(funcObj, ns);
        *bodyTok = head;
        return funcObj;
    }
    #undef RETVAL
}

#undef REQUIRE_MORE_TOKENS