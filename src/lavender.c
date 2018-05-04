#include "lavender.h"
#include "token.h"
#include "operator.h"
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
        while(true) {
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
}

void lv_shutdown() {
    
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
        printf("Type %d, value %s\n", tmp->type, tmp->value);
        tmp = tmp->next;
    }
    if(toks) {
        Token* t = lv_op_declareFunction(toks);
        if(LV_OP_ERROR) {
            printf("Error parsing function: %s\n", lv_op_getError(LV_OP_ERROR));
            LV_OP_ERROR = 0;
        } else {
            printf("First token of body: type=%d, value=%s\n",
                t->type,
                t->value);
        }
    }
    lv_tkn_free(toks);
}
