#include "expression.h"
#include "textbuffer.h"
#include "operator.h"
#include "lavender.h"
#include <assert.h>
#include <string.h>

bool lv_expr_isReserved(char* id) {
    return strcmp(id, "def") == 0
        || strcmp(id, "let") == 0
        || strcmp(id, "do") == 0
        || strcmp(id, "native") == 0
        || strcmp(id, "=>") == 0
        || strcmp(id, "<-") == 0;
}

char* lv_expr_getError(ExprError error) {
    #define LEN (sizeof(msg) / sizeof(char*))
    static char* msg[] = {
        "Expr does not define a function",
        "Reached end of input while parsing",
        "Expected an argument list",
        "Malformed argument list",
        "Missing function body",
        "Duplicate function definition",
        "Function name not found",
        "Expected operator",
        "Expected operand",
        "Encountered unexpected token",
        "Unbalanced parens or brackets",
        "Wrong number of parameters to function",
        "Function arity incompatible with fixing",
        "Malformed function local list",
        "Identifier is reserved",
        "Native function declares locals",
        "Native implementation not found"
    };
    assert(error > 0 && error <= LEN);
    return msg[error - 1];
    #undef LEN
}

void lv_expr_cleanup(TextBufferObj* obj, size_t len) {

    for(size_t i = 0; i < len; i++) {
        if(obj[i].type == OPT_STRING) {
            assert(obj[i].str->refCount);
            if(--obj[i].str->refCount == 0)
                lv_free(obj[i].str);
        } else if(obj[i].type == OPT_CAPTURE) {
            assert(obj[i].capture->refCount);
            if(--obj[i].capture->refCount == 0) {
                lv_expr_cleanup(obj[i].capture->value, obj[i].capfunc->captureCount);
                lv_free(obj[i].capture);
            }
        } else if(obj[i].type == OPT_VECT) {
            assert(obj[i].vect->refCount);
            if(--obj[i].vect->refCount == 0) {
                lv_expr_cleanup(obj[i].vect->data, obj[i].vect->len);
                lv_free(obj[i].vect);
            }
        }
    }
}

void lv_expr_free(TextBufferObj* obj, size_t len) {

    lv_expr_cleanup(obj, len);
    lv_free(obj);
}
