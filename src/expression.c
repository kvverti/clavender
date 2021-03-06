#include "expression.h"
#include "textbuffer.h"
#include "operator.h"
#include "lavender.h"
#include <assert.h>
#include <string.h>

bool lv_expr_isReserved(char* id, size_t len) {
    switch(len) {
        case 3:
            return strncmp(id, "def", len) == 0
                || strncmp(id, "let", len) == 0;
        case 2:
            return strncmp(id, "do", len) == 0
                || strncmp(id, "=>", len) == 0
                || strncmp(id, "<-", len) == 0;
        case 6:
            return strncmp(id, "native", len) == 0;
        case 1:
            return id[0] == '_'
                || id[0] == ':';
        default:
            return false;
    }
}

char* lv_expr_getError(ExprError error) {
    #define LEN (sizeof(msg) / sizeof(char*))
    static char* msg[] = {
        "Expr does not define a function",
        "Reached end of input while parsing",
        "Expected an argument list",
        "Malformed argument list",
        "Exceeded maximum parameter count",
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
        "Native implementation not found",
        "Cannot take value of zero arity function"
    };
    assert(error > 0 && error <= LEN);
    return msg[error - 1];
    #undef LEN
}

void lv_expr_cleanup(TextBufferObj* obj, size_t len) {

    for(size_t i = 0; i < len; i++) {
        switch(obj[i].type) {
            case OPT_STRING:
                assert(obj[i].str->refCount);
                if(--obj[i].str->refCount == 0)
                    lv_free(obj[i].str);
                break;
            case OPT_CAPTURE:
                assert(obj[i].capture->refCount);
                if(--obj[i].capture->refCount == 0) {
                    lv_expr_cleanup(obj[i].capture->value, obj[i].capfunc->captureCount);
                    lv_free(obj[i].capture);
                }
                break;
            case OPT_VECT:
                assert(obj[i].vect->refCount);
                if(--obj[i].vect->refCount == 0) {
                    lv_expr_cleanup(obj[i].vect->data, obj[i].vect->len);
                    lv_free(obj[i].vect);
                }
                break;
            case OPT_MAP:
                assert(obj[i].map->refCount);
                if(--obj[i].map->refCount == 0) {
                    size_t s = obj[i].map->len;
                    for(size_t j = 0; j < s; j++) {
                        lv_expr_cleanup(&obj[i].map->data[j].key, 1);
                        lv_expr_cleanup(&obj[i].map->data[j].value, 1);
                    }
                    lv_free(obj[i].map);
                }
                break;
            default:
                ;
        }
    }
}

void lv_expr_free(TextBufferObj* obj, size_t len) {

    lv_expr_cleanup(obj, len);
    lv_free(obj);
}
