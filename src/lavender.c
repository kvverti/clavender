#include "lavender.h"
#include "expression.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

bool lv_debug = false;
char* lv_filepath = ".";
char* lv_mainFile = NULL;

static void readInput(FILE* in, bool repl);

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
    
    lv_op_onStartup();
    lv_tb_onStartup();
}

void lv_shutdown() {
    
    lv_tb_onShutdown();
    lv_op_onShutdown();
    exit(0);
}

void lv_repl() {
    
    readInput(stdin, true);
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
        Operator* op;
        memset(&scope, 0, sizeof(Operator));
        scope.name = "repl";
        scope.type = FUN_FWD_DECL;  //because its a "declaration"
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
    }
    lv_tkn_free(toks);
}
