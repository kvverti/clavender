#include "lavender.h"
#include "expression.h"
#include "builtin.h"
#include "command.h"
#include "dynbuffer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

bool lv_debug = false;
char* lv_filepath = ".";
char* lv_mainFile = NULL;

static void readInput(FILE* in, bool repl);
static void runCycle(void);

static DynBuffer stack; //of TextBufferObj
static size_t pc;   //program counter
static size_t fp;   //frame pointer: index of the first argument

static void push(TextBufferObj* obj) {
    
    if(obj->type == OPT_STRING)
        obj->str->refCount++;
    lv_buf_push(&stack, obj);
}

static void popAll(size_t numToPop) {
    
    TextBufferObj* start = lv_buf_get(&stack, stack.len - numToPop);
    lv_expr_cleanup(start, numToPop);
    stack.len -= numToPop;
}

static TextBufferObj removeTop(void) {
    
    TextBufferObj res;
    lv_buf_pop(&stack, &res);
    if(res.type == OPT_STRING)
        res.str->refCount--;
    return res;
}

//simple linear storage should be enough
//for the relatively small number of namespaces
static DynBuffer importedFiles; //of char*

static bool addFile(char* file) {
    
    for(size_t i = 0; i < importedFiles.len; i++) {
        char* str = *(char**)lv_buf_get(&importedFiles, i);
        if(strcmp(str, file) == 0)
            return false; //already imported
    }
    size_t len = strlen(file) + 1;
    char* tmp = lv_alloc(len);
    memcpy(tmp, file, len);
    lv_buf_push(&importedFiles, &tmp);
    return true;
}

void lv_run(void) {
    
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
    if(!value) {
        printf("Allocation failed: %lu bytes\n", size);
        lv_shutdown();
    }
    return value;
}

void* lv_realloc(void* ptr, size_t size) {
    
    void* tmp = realloc(ptr, size);
    if(!tmp) {
        free(ptr);
        printf("Allocation failed: %lu bytes\n", size);
        lv_shutdown();
    }
    return tmp;
}

void lv_free(void* ptr) {
    
    free(ptr);
}

void lv_startup(void) {
    
    pc = fp = 0;
    lv_buf_init(&stack, sizeof(TextBufferObj));
    lv_buf_init(&importedFiles, sizeof(char*));
    lv_op_onStartup();
    lv_tb_onStartup();
    lv_blt_onStartup();
    lv_cmd_onStartup();
}

void lv_shutdown(void) {
    
    lv_cmd_onShutdown();
    lv_blt_onShutdown();
    lv_tb_onShutdown();
    lv_op_onShutdown();
    lv_expr_cleanup(stack.data, stack.len);
    for(size_t i = 0; i < importedFiles.len; i++) {
        lv_free(*(char**)lv_buf_get(&importedFiles, i));
    }
    lv_free(importedFiles.data);
    lv_free(stack.data);
    exit(0);
}

void lv_repl(void) {
    
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

/** Helper struct to organize function declaration data. */
typedef struct HelperDeclObj {
    Operator* func;
    Token* body;
} HelperDeclObj;

static bool getFuncSig(FILE* file, Operator* scope, DynBuffer* decls);

bool lv_readFile(char* name) {
    
    if(!addFile(name))
        return true; //nothing to do..
    //open file
    //TODO clean up a bit (i.e. less strlen)
    static char ext[] = ".lv";  //lv_filepath/name.lv
    char* file = lv_alloc(strlen(lv_filepath) + 1 + strlen(name) + sizeof(ext));
    strcpy(file, lv_filepath);
    strcat(file, "/");  // '/' works on all major OS (Windows, Mac, Linux)
    strcat(file, name);
    strcat(file, ext);
    //try the current directory first, then the Lavender filepath
    FILE* importFile = fopen(file + strlen(lv_filepath) + 1, "r");
    if(!importFile) {
        importFile = fopen(file, "r");
        if(!importFile) {
            lv_free(file);
            return false;
        }
    }
    //end open file
    //parse file
    DynBuffer decls;    //of HelperDeclObj
    lv_buf_init(&decls, sizeof(HelperDeclObj));
    bool res = true;
    Operator scope;
    memset(&scope, 0, sizeof(Operator));
    scope.name = name;
    scope.type = FUN_FWD_DECL;
    //parse all function declarations
    while(!feof(importFile) && res) {
        res = getFuncSig(importFile, &scope, &decls);
    }
    //successful parse of all declarations
    if(res) {
        //parse function definitions
        for(size_t i = 0; i < decls.len; i++) {
            HelperDeclObj* obj = lv_buf_get(&decls, i);
            lv_tb_defineFunctionBody(obj->body, obj->func);
            if(LV_EXPR_ERROR) {
                res = false;
                printf("Error parsing function body: %s\n", lv_expr_getError(LV_EXPR_ERROR));
                LV_EXPR_ERROR = 0;
                break;
            }
        }
    }
    for(size_t i = 0; i < decls.len; i++) {
        HelperDeclObj* obj = lv_buf_get(&decls, i);
        lv_tkn_free(obj->body);
    }
    lv_free(decls.data);
    fclose(importFile);
    lv_free(file);
    return res;
}

static bool getFuncSig(FILE* file, Operator* scope, DynBuffer* decls) {
    
    Token* head = lv_tkn_split(file);
    if(LV_TKN_ERROR) {
        printf("Error parsing input: %s\nHere: '%s'\n",
            lv_tkn_getError(LV_TKN_ERROR), lv_tkn_errcxt);
        LV_TKN_ERROR = 0;
        return false;
    }
    if(!head) {
        //empty line
        return true;
    }
    if(!isFuncDef(head)) {
        //must be function definitions
        puts("Error parsing input: Not a function definition");
        lv_tkn_free(head);
        return false;
    }
    //declare function
    Token* body;
    Operator* op = lv_expr_declareFunction(head, scope, &body);
    if(LV_EXPR_ERROR) {
        printf("Error parsing function signatures: %s\n",
            lv_expr_getError(LV_EXPR_ERROR));
        LV_EXPR_ERROR = 0;
        lv_tkn_free(head);
        return false;
    }
    //push declaration and body pointer
    HelperDeclObj toPush = { op, body };
    lv_buf_push(decls, &toPush);
    //free decl tokens only
    Token* tmp = head;
    while(tmp->next != body)
        tmp = tmp->next;
    tmp->next = NULL;
    lv_tkn_free(head);
    return true;
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
    if(toks) {
        //it's not going into the list, auto alloc is fine
        Operator scope;
        memset(&scope, 0, sizeof(Operator));
        scope.type = FUN_FWD_DECL;  //because its a "declaration"
        if(toks->value[0] == '@') {
            Token* cmd = toks->next;
            lv_free(toks); //free '@' token because we may not return
            lv_cmd_run(cmd);
            puts(lv_cmd_message);
            toks = cmd; //so it's freed later
        } else if(isFuncDef(toks)) {
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
                TextBufferObj obj = removeTop();
                LvString* str = lv_tb_getString(&obj);
                puts(str->value);
                if(str->refCount == 0)
                    lv_free(str);
                if(obj.type == OPT_CAPTURE)
                    lv_expr_free(obj.capture.captured, obj.capture.func->captureCount);
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

static void runCycle(void) {
    
    TextBufferObj* value = &TEXT_BUFFER[pc++];
    TextBufferObj func; //used in OPT_FUNC_CALL
    switch(value->type) {
        case OPT_FUNCTION_VAL: {
            //capture outer arguments into function object
            if(value->func->captureCount > 0) {
                //fp points to first argument of the outer function
                assert(value->func->type == FUN_FUNCTION);
                TextBufferObj obj;
                obj.type = OPT_CAPTURE;
                obj.capture.func = value->func;
                obj.capture.captured = lv_alloc(value->func->captureCount * sizeof(TextBufferObj));
                for(int i = 0; i < value->func->captureCount; i++) {
                    obj.capture.captured[i] = *(TextBufferObj*)lv_buf_get(&stack, fp + i);
                    if(obj.capture.captured[i].type == OPT_STRING)
                        obj.capture.captured[i].str->refCount++;
                }
                push(&obj);
                break;
            } else;
            //fallthrough
        }
        case OPT_UNDEFINED:
        case OPT_NUMBER:
        case OPT_STRING:
        case OPT_CAPTURE:
            //push it on the stack
            push(value);
            break;
        case OPT_PARAM:
            //push i'th param
            push(lv_buf_get(&stack, fp + value->param));
            break;
        case OPT_BEQZ: {
            TextBufferObj obj = removeTop();
            if(!lv_blt_toBool(&obj))
                pc += value->branchAddr - 1;
            break;
        }
        case OPT_FUNC_CALL: {
            func = removeTop();
            //todo: handle strings
            if(func.type == OPT_FUNCTION_VAL && value->callArity == func.func->arity) {
                value = &func;
                assert(value->type == OPT_FUNCTION_VAL);
            } else if(func.type == OPT_CAPTURE
                && value->callArity == (func.capture.func->arity - func.capture.func->captureCount)) {
                //move captured params to stack
                for(int i = 0; i < func.capture.func->captureCount; i++) {
                    lv_buf_push(&stack, &func.capture.captured[i]);
                }
                //free memory for captured because we are destroying the capture obj
                //we don't need to clean up individual elements because we transfered
                //ownership to the stack
                lv_free(func.capture.captured);
                func.func = func.capture.func;
                value = &func;
            } else {
                //can't call, pop args and push undefined
                popAll(value->callArity);
                TextBufferObj nan;
                nan.type = OPT_UNDEFINED;
                push(&nan);
                //free capture memory / string memory
                if(func.type == OPT_CAPTURE)
                    lv_expr_free(func.capture.captured, func.capture.func->captureCount);
                else if(func.type == OPT_STRING && func.str->refCount == 0)
                    lv_free(func.str);
                return;
            }
            //fallthrough
        }
        case OPT_FUNCTION: {
            switch(value->func->type) {
                case FUN_FWD_DECL: {
                    //this should never happen
                    assert(false);
                    break;
                }
                case FUN_BUILTIN: {
                    //call built in function, then pop args and push result
                    size_t tmpFp = stack.len - value->func->arity;
                    TextBufferObj res = value->func->builtin(lv_buf_get(&stack, tmpFp));
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
            //bypass removeTop for the return value
            //so we keep its string refCount intact
            //this keeps popAll from freeing the return value
            TextBufferObj retVal;
            lv_buf_pop(&stack, &retVal);
            //reset pc and fp
            pc = removeTop().addr;
            size_t tmpFp = removeTop().addr;
            //pop args
            popAll(stack.len - fp);
            fp = tmpFp;
            lv_buf_push(&stack, &retVal);
            break;
        }
        default:
            //set error
            return;
    }
}
