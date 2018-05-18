#include "lavender.h"
#include "expression.h"
#include "builtin.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

bool lv_debug = false;
char* lv_filepath = ".";
char* lv_mainFile = NULL;

static void readInput(FILE* in, bool repl);
static void runCycle();

#define INIT_STACK_SIZE 16
static struct DataStack {
    TextBufferObj* data;
    size_t cap;     //total capacity of stack
    size_t len;     //size of stack
} stack;
static size_t pc;   //program counter
static size_t fp;   //frame pointer: index of the first argument

static void push(TextBufferObj* obj) {
    
    if(stack.len == stack.cap) {
        stack.data = lv_realloc(stack.data, stack.cap * 2 * sizeof(TextBufferObj));
        stack.cap *= 2;
    }
    stack.data[stack.len++] = *obj;
}

static void popAll(size_t numToPop) {
    
    TextBufferObj* start = stack.data + (stack.len - numToPop);
    //lv_expr_cleanup(start, numToPop);
    stack.len -= numToPop;
}

#define STACK_REMOVE_TOP (stack.data[--stack.len])

void lv_run() {
    
    lv_startup();
    if(lv_mainFile) {
        char ext[] = ".lv";
        char* filename = lv_alloc(strlen(lv_mainFile) + sizeof(ext));
        filename[0] = '\0';
        strcat(filename, lv_mainFile);
        strcat(filename, ext);
        FILE* file = fopen(filename, "r");
        if(!file) {
            printf("Cannot read main file %s\n", filename);
        } else {
            while(!feof(file))
                readInput(file, false);
            fclose(file);
        }
        lv_free(filename);
    } else {
        if(lv_debug)
            puts("Running in debug mode");
        puts("Lavender runtime v. 1.0 by Chris Nero\n"
             "Open source at https://github.com/kvverti/clavender\n"
             "Enter function definitions or expressions");
        while(!feof(stdin)) {
            lv_repl();
        }
    }
    lv_shutdown();
}

void* lv_alloc(size_t size) {
    
    void* value = malloc(size);
    if(!value)
        lv_shutdown();
    return value;
}

void* lv_realloc(void* ptr, size_t size) {
    
    void* tmp = realloc(ptr, size);
    if(!tmp) {
        free(ptr);
        lv_shutdown();
    }
    return tmp;
}

void lv_free(void* ptr) {
    
    free(ptr);
}

void lv_startup() {
    
    stack.data = lv_alloc(INIT_STACK_SIZE * sizeof(TextBufferObj));
    stack.cap = INIT_STACK_SIZE;
    stack.len = 0;
    pc = fp = 0;
    lv_op_onStartup();
    lv_tb_onStartup();
    lv_blt_onStartup();
}

void lv_shutdown() {
    
    lv_blt_onShutdown();
    lv_tb_onShutdown();
    lv_op_onShutdown();
    lv_free(stack.data);
    exit(0);
}

void lv_repl() {
    
    readInput(stdin, true);
}

static bool isFuncDef(Token* head) {
    
    //functions may have one level of parens
    assert(head);
    if(head->value[0] == '(') {
        head = head->next;
    }
    return head && strcmp(head->value, "def") == 0;
}

static void readInput(FILE* in, bool repl) {
    
    if(repl)
        printf("> ");
    Token* toks = lv_tkn_split(in);
    if(LV_TKN_ERROR) {
        printf("Error parsing input: %s\nHere: '%s'\n",
            lv_tkn_getError(LV_TKN_ERROR), lv_tkn_errcxt);
        LV_TKN_ERROR = 0;
        return;
    }
    Token* tmp = toks;
    while(tmp) {
        if(strcmp(tmp->value, "@") == 0) {
            lv_tkn_free(toks);
            lv_shutdown();
        }
        tmp = tmp->next;
    }
    if(toks) {
        //it's not going into the list, auto alloc is fine
        Operator scope;
        memset(&scope, 0, sizeof(Operator));
        scope.type = FUN_FWD_DECL;  //because its a "declaration"
        if(isFuncDef(toks)) {
            //define function
            Operator* op;
            scope.name = "repl";    //namespace
            Token* end = lv_tb_defineFunction(toks, &scope, &op);
            if(LV_EXPR_ERROR) {
                printf("Error parsing function: %s\n",
                    lv_expr_getError(LV_EXPR_ERROR));
                LV_EXPR_ERROR = 0;
            } else {
                puts(op->name);
                if(end)
                    printf("First token past body: type=%d, value=%s\n",
                        end->type,
                        end->value);
            }
        } else {
            //parse expression
            scope.name = "repl:$";      //expr requires a named function
            size_t startIdx, endIdx;
            Token* end = lv_tb_parseExpr(toks, &scope, &startIdx, &endIdx);
            if(LV_EXPR_ERROR) {
                printf("Error parsing expression: %s\n",
                    lv_expr_getError(LV_EXPR_ERROR));
                LV_EXPR_ERROR = 0;
            } else {
                pc = startIdx;
                while(pc != endIdx) {
                    runCycle();
                }
                assert(stack.len == 1);
                LvString* str = lv_tb_getString(&STACK_REMOVE_TOP);
                puts(str->value);
                lv_free(str);
                lv_tb_clearExpr();
                if(end)
                    printf("First token past body: type=%d, value=%s\n",
                        end->type,
                        end->value);
            }
        }
    }
    lv_tkn_free(toks);
}

static void runCycle() {
    
    TextBufferObj* value = &TEXT_BUFFER[pc++];
    switch(value->type) {
        case OPT_UNDEFINED:
        case OPT_NUMBER:
        case OPT_STRING:
        case OPT_FUNCTION_VAL:
            //push it on the stack
            push(value);
            break;
        case OPT_PARAM:
            //push i'th param
            push(&stack.data[fp + value->param]);
            break;
        case OPT_BEQZ:
            value += value->branchAddr - 1;
            break;
        case OPT_FUNC_CALL: {
            TextBufferObj* func = &STACK_REMOVE_TOP;
            if(value->callArity != func->func->arity) {
                //can't call, pop args and push undefined
                popAll(value->callArity);
                TextBufferObj nan;
                nan.type = OPT_UNDEFINED;
                push(&nan);
                return;
            }
            value = func;
            //fallthrough
        }
        case OPT_FUNCTION: {
            switch(value->func->type) {
                case FUN_FWD_DECL: {
                    //handle error
                    break;
                }
                case FUN_BUILTIN: {
                    //call built in function, then pop args and push result
                    size_t tmpFp = stack.len - value->func->arity;
                    TextBufferObj res = value->func->builtin(stack.data + tmpFp);
                    popAll(value->func->arity);
                    push(&res);
                    break;
                }
                case FUN_FUNCTION: {
                    //calling convention
                    //  1. push fp
                    //  2. set fp = stack.len - func.arity - 1 (first argument)
                    //  3. push pc (return value)
                    //  4. set pc = first inst of function
                    //The stack looks like this:
                    //  ... arg0 arg1 .. argN-1 fp pc ...
                    //       ^^
                    //       fp
                    TextBufferObj obj;
                    obj.type = OPT_ADDR;
                    obj.addr = fp;
                    push(&obj);
                    fp = stack.len - value->func->arity - 1;
                    obj.addr = pc;
                    push(&obj);
                    pc = value->func->textOffset;
                    break;
                }
            }
            break;
        }
        case OPT_RETURN: {
            TextBufferObj* retVal = &STACK_REMOVE_TOP;
            //reset pc and fp
            pc = STACK_REMOVE_TOP.addr;
            size_t tmpFp = STACK_REMOVE_TOP.addr;
            //pop args
            popAll(stack.len - fp);
            fp = tmpFp;
            push(retVal);
            break;
        }
        default:
            //set error
            return;
    }
}
