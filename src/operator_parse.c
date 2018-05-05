#include "operator.h"
#include "lavender.h"
#include <string.h>
#include <assert.h>

#define REQUIRE_MORE_TOKENS(x) \
    if(!(x)) { LV_OP_ERROR = OPE_UNTERM_EXPR; return RETVAL; } else (void)0
    
char* lv_op_getError(OpError error) {
    #define LEN 10
    static char* msg[LEN] = {
        "Expr does not define a function",
        "Reached end of input while parsing",
        "Expected an argument list",
        "Malformed argument list",
        "Missing function body",
        "Duplicate function definition",
        "Simple function name not found",
        "Expected operator",
        "Expected operand",
        "Encountered unexpected token"
    };
    assert(error > 0 && error <= LEN);
    return msg[error - 1];
    #undef LEN
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

static Token* pe_head;        //the current token
static Operator* pe_decl;     //the current function declaration
static char* pe_startOfName;  //the beginning of the simple name
static bool pe_expectOperand; //do we expect an operand or an operator

static void parseLiteral(TextBufferObj* obj);
static void parseIdent(TextBufferObj* obj);
static void parseFunction(TextBufferObj* obj);
static void parseNumber(TextBufferObj* obj);
static void parseString(TextBufferObj* obj);

static int exprLength(Token* head) {
    
    //nesting depth of parens and brackets
    //when this goes negative, the expression is ended
    int nesting = 0;
    int res = 0;
    while(head) {
        res++;
        if(head->type == TTY_LITERAL) {
            switch(head->value[0]) {
                case '(':
                case '[': nesting++; break;
                case ')':
                case ']': nesting--; break;
            }
            if(nesting < 0) {
                //end of expr!
                return res;
            }
        }
        head = head->next;
    }
    return res;
}

static char* concat(char* a, int alen, char* b, int blen) {
    
    char* res = lv_alloc(alen + blen + 1);
    memcpy(res, a, alen);
    memcpy(res + alen, b, blen);
    res[alen + blen] = '\0';
    return res;
}

static void parseLiteral(TextBufferObj* obj) {
    
    obj->type = OPT_LITERAL;
    obj->literal = pe_head->value[0];
    switch(obj->literal) {
        case '(':
        case '[':
            //open groupings are "operands"
            if(!pe_expectOperand) {
                LV_OP_ERROR = OPE_EXPECT_PRE;
            }
            break;
        case ')':
        case ']':
        case ',':
            //close groupings are "operators"
            if(pe_expectOperand) {
                LV_OP_ERROR = OPE_EXPECT_INF;
            }
            break;
        default:
            LV_OP_ERROR = OPE_UNEXPECT_TOKEN;
    }
}

static void parseFunction(TextBufferObj* obj) {
    
    FuncNamespace ns = pe_expectOperand ? FNS_PREFIX : FNS_INFIX;
    //not a parameter, try simple names
    //we find the function with the simple name
    //in the innermost scope possible by going
    //through outer scopes until we can't find
    //a function definition.
    int valueLen = strlen(pe_head->value);
    char* nsbegin = pe_decl->name;
    Operator* func;
    Operator* test = NULL;
    do {
        func = test;
        if(pe_startOfName == nsbegin)
            break; //we did all the namespaces
        //get the function with the name in the scope
        char* fname = concat(nsbegin,   //beginning of scope
            pe_startOfName - nsbegin,   //length of scope name
            pe_head->value,                //name to check
            valueLen);                  //length of name
        test = lv_op_getOperator(fname, ns);
        lv_free(fname);
        nsbegin = strchr(nsbegin, ':') + 1;
    } while(test);
    //test is null. func should contain the function
    if(!func) {
        //that name does not exist!
        LV_OP_ERROR = OPE_NAME_NOT_FOUND;
        return;
    }
    obj->type = OPT_FUNCTION;
    obj->func = func;
    //toggle if RHS is true
    pe_expectOperand ^= (!pe_expectOperand || func->arity == 0);
}

static void parseIdent(TextBufferObj* obj) {
    
    if(pe_expectOperand) {
        //is this a def?
        if(strcmp(pe_head->value, "def") == 0) {
            //todo
        }
        //try parameter names first
        for(int i = 0; i < pe_decl->arity; i++) {
            if(strcmp(pe_head->value, pe_decl->params[i].name) == 0) {
                //save param name
                obj->type = OPT_PARAM;
                obj->param = i;
                pe_expectOperand = false;
                return;
            }
        }
    }
    //not a parameter, try a function
    parseFunction(obj);
}

void lv_op_parseExpr(Token* head, Operator* decl, TextBufferObj** res, size_t* len) {
    
    size_t sz = exprLength(head);
    //text object vector
    TextBufferObj* expr = lv_alloc(*len * sizeof(TextBufferObj));
    //set up global vars
    pe_head = head;
    pe_decl = decl;
    pe_startOfName = strrchr(decl->name, ':') + 1;
    pe_expectOperand = true;
    for(int i = 0; i < sz; i++, pe_head = pe_head->next) {
        //let the 76'th hunger games begin
        switch(pe_head->type) {
            case TTY_LITERAL:
                parseLiteral(&expr[i]);
                break;
            case TTY_IDENT:
                parseIdent(&expr[i]);
                break;
            case TTY_SYMBOL:
                parseFunction(&expr[i]);
                break;
            default:
                //stub
                expr[i].type = OPT_LITERAL;
                expr[i].literal = '?';
        }
        if(LV_OP_ERROR) {
            lv_free(expr);
            return;
        }
    }
    *res = expr;
    *len = sz;
}

#undef REQUIRE_MORE_TOKENS
