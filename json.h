#ifndef _JSON_H_
#define _JSON_H_

#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>


/* bad attempt, lets try the good ol original again! */

enum json_sig {
    JSON_NUM,
    JSON_BOOL,
    JSON_NULL,
    JSON_STR,
    JSON_ARRAY,
    JSON_OBJECT,
    JSON_END,
    JSON_ERROR,
    JSON_INITIAL,
    JSON_HALT,
};

enum json_ctx {
    JSON_CTX_VALUE,
    JSON_CTX_ARRAY,
    JSON_CTX_OBJECT_KEY,
    JSON_CTX_OBJECT_VALUE,
};


/*
Grammar: (* before rule means uses stack)

initial -> value

value -> NUM | BOOL | NULL | STR | array | object

*array -> ARRAY value* END

*object -> OBJECT (STR value)* END
*/
struct json_head {
    enum json_ctx ctx;
    struct {
        enum json_ctx * begin, * ptr, * end;
        int realloc;
        int depth;
    } stack;

    enum json_sig sig;
    char * begin, * end;
    char * valid_end;
    union json_valcache {
        int errv;
        int boolv;
        size_t strlen;
    } valcache;

};

typedef struct json_mark {
    enum json_ctx ctx;
    int depth;
    enum json_sig sig;
    char * begin, * end;
    union json_valcache valcache;
} json_mark;



static void json_init(struct json_head * head, char * str, enum json_ctx * stack, size_t stack_size);
static void json_next(struct json_head * head);
static int json_get(struct json_head * head, const char * query, ...);
static void json_restore(struct json_head * head, json_mark * mark);


static void json_init(struct json_head * head, char * str, enum json_ctx * stack, size_t stack_size) {
    if(!stack) {
        stack_size = stack_size > 0? stack_size : 16;
        stack = malloc(sizeof(enum json_ctx) * stack_size);
        head->stack.realloc = 1;
    } else {
        head->stack.realloc = 0;
    }

    head->ctx = JSON_CTX_VALUE;
    head->sig = JSON_INITIAL;
    head->stack.begin = stack;
    head->stack.ptr = stack;
    head->stack.end = stack + stack_size;
    head->stack.depth = 0;
    head->end = str;
    head->valid_end = str;

    json_next(head);
}

static char * json_bump(char * str) {
    while(isspace(*str)) str++;
    return str;
}

static void json_push(struct json_head * head, enum json_ctx ctx) {
    *head->stack.ptr++ = head->ctx;
    head->ctx = ctx;
    head->stack.depth++;
}

static void json_pop(struct json_head * head) {
    head->ctx = *--head->stack.ptr;
    head->stack.depth--;
}

static void json_next(struct json_head * head) {
#define BUMP c = *(p = json_bump(p));
#define SKIP(n) c = *(p += n);

    char * p = head->end;
    char c;
    int check_validity = p == head->valid_end;
    
    BUMP
    
    head->begin = p;

    switch(head->sig) {
        case JSON_NUM:
        case JSON_BOOL:
        case JSON_STR:
        case JSON_NULL:
        case JSON_END: switch(head->ctx) {
            case JSON_CTX_ARRAY: {
                if(c == ']') {
                    head->sig = JSON_END;
                    head->end = p + 1;
                    json_pop(head);
                    goto finish;
                } else if (c == ',') {
                    SKIP(1)
                    BUMP

                    head->begin = p;
                    goto read_value;
                } else {
                    goto error;
                }
            } break;

    	    case JSON_CTX_OBJECT_KEY: {
                if(c != ':') {
                    goto error;
                }

                SKIP(1)
                BUMP

                head->begin = p;
                head->ctx = JSON_CTX_OBJECT_VALUE;

                goto read_value;
            } break;

    	    case JSON_CTX_OBJECT_VALUE: {
                if(c == '}') {
                    head->sig = JSON_END;
                    head->end = p + 1;

                    json_pop(head);
                    
                    goto finish;
                } else if(c == ',') {
                    SKIP(1) BUMP

                    head->begin = p;
                    head->ctx = JSON_CTX_OBJECT_KEY;

                    goto read_string_value;
                } else {
                    goto error;
                }
            } break;         

            case JSON_CTX_VALUE: {
                json_pop(head);
                head->sig = JSON_HALT;
                goto finish;
            }
        } break;

        case JSON_ARRAY: {
            if(c == ']') { /* case [] */
                head->sig = JSON_END;
                head->end = p + 1;
                goto finish;
            } else {
                json_push(head, JSON_CTX_ARRAY);
                goto read_value;
            }
        } break;

        case JSON_OBJECT: {
            if(c == '}') { /* case {} */
                head->sig = JSON_END;
                head->end = p + 1;
                goto finish;
            } else {
                json_push(head, JSON_CTX_OBJECT_KEY);
                goto read_string_value;
            }
        } break;

        case JSON_ERROR: {
            goto finish;
        } break;

        case JSON_HALT: goto finish;
        case JSON_INITIAL: {
            goto read_value;
        } break;
    }

    

read_value:;
    if(c == '[') {
        head->sig = JSON_ARRAY;
        head->end = p + 1;
        goto finish;
    } else if(c == '{') {
        head->sig = JSON_OBJECT;
        head->end = p + 1;
        goto finish;
    } else if(memcmp(p, "true", 4) == 0) {
        head->sig = JSON_BOOL;
        head->valcache.boolv = 1;
        head->end = p + 4;
        goto finish;
    } else if(memcmp(p, "false", 5) == 0) {
        head->sig = JSON_BOOL;
        head->valcache.boolv = 0;
        head->end = p + 5;
        goto finish;
    } else if(memcmp(p, "null", 4) == 0) {
        head->sig = JSON_NULL;
        head->end = p + 4;
        goto finish;
    }

    while(1) switch(c) {
        case 'e': case 'E': case '.':
        case '0': case '1': case '2': case '3': case '4': case '5': case '6':
        case '7': case '8': case '9': case '+': case '-': {
            SKIP(1);
        } break;

        default: {
            /* end? */
            if(head->begin != p) {
                /* means we skipped something at least once */
                head->sig = JSON_NUM;
                head->end = p;
                goto finish;
            } else {
                /* else: this isnt a number */
                goto read_string_value;
            }
        }
    }

read_string_value:;
    if(c != '"') {
        goto error;
    }

    SKIP(1);

    head->valcache.strlen = 0;

    while(c != '"') {
        if(c == '\\') {
            SKIP(1);
            if(c == 'u') {
                /* TODO: utf8 here! */
                SKIP(4);
            }
        }
        SKIP(1);
        head->valcache.strlen++;
    }

    SKIP(1);
    head->end = p;
    head->sig = JSON_STR;
    goto finish;

error:
    head->sig = JSON_ERROR;

finish:
    if(head->end > head->valid_end)
        head->valid_end = head->end;
    return;
}

static void json_skip(struct json_head * head) {
    int stackdepth = head->stack.depth;
    for(;;) {
        switch(head->sig) {
            case JSON_HALT:
            case JSON_ERROR: return;
            case JSON_BOOL:
            case JSON_NULL:
            case JSON_STR:
            case JSON_NUM:
            case JSON_END: if(head->stack.depth == stackdepth) goto next;
            default: json_next(head); break;
        }
    }

next:
    json_next(head);
}

static int json_get(struct json_head * head, const char * query, ...) {
    char * p, * endp;
    va_list va;


    int intv;
    float floatv;
    struct {
        size_t cap;
        char * buf;
    } strv;
    json_mark * mark;

    int rc = 0;

    va_start(va, query);


    while(1) {
        switch(*query) {

        case 't': {
            /* get type */
            *va_arg(va, enum json_sig *) = head->sig;
        } break;

        case 'i': {
            /* get as integer */
            switch(head->sig) {

            case JSON_BOOL: intv = head->valcache.boolv; break;
            case JSON_NULL: intv = 0; break;
            case JSON_NUM: {
                intv = strtol(head->begin, NULL, 10);
            } break;
            default: goto error;

            }

            *va_arg(va, int *) = intv;
        } break;

        case 'f': {
            /* get as float */
            floatv = (float)strtod(head->begin, NULL);
            *va_arg(va, float *) = floatv;
        } break;

        case 's': {
            /* copy "raw" contents to some buffer (string: copy to buffer) */
            strv.buf = va_arg(va, char *);
            strv.cap = va_arg(va, size_t);

            if((head->end - head->begin - 2) == head->valcache.strlen) {
                /* no escape sequences, thats good */
                memcpy(strv.buf, head->begin + 1, head->valcache.strlen);
                /* add a Zero character if possible */
                if(strv.cap > head->valcache.strlen)
                    strv.buf[head->valcache.strlen] = '\0';
            } else {
                /* some escape sequences */
                for(p = head->begin + 1, endp = head->end - 1; p < endp && strv.cap > 0; ) {
                    if(*p == '\\') {
                        switch(*++p) {

                        case 'f': *strv.buf++ = '\f'; p++; break;
                        case 'r': *strv.buf++ = '\r'; p++; break;
                        case 't': *strv.buf++ = '\t'; p++; break;
                        case 'n': *strv.buf++ = '\n'; p++; break;
                        case 'v': *strv.buf++ = '\v'; p++; break;
                        case 'b': *strv.buf++ = '\b'; p++; break;
                        case '/': *strv.buf++ = '/'; p++; break;
                        case '\\': *strv.buf++ = '\\'; p++; break;
                        case '\"': *strv.buf++ = '\"'; p++; break;
                        case 'u': {
                            /* TODO: implement! */
                            *strv.buf++ = 'u';
                            p += 5;
                        } break;
                        default: /* TODO: thats an error! */ p++; break;

                        }
                    } else {
                        *strv.buf++ = *p++;
                    }

                    strv.cap--;
                }

                /* add a zero character if possible */
                if(strv.cap > 0)
                    *strv.buf++ = '\0';
            }
        } break;

        case '#': {
            /* meaning differs on the sig:
             * array/object: calculate length,
             * num/null/bool: integer/float size,
             * string: string size
             * error: ??? */
            switch(head->sig) {
            
            case JSON_STR: *va_arg(va, size_t *) = head->valcache.strlen;
            default: goto error;

            }
        } break;

        case '[': {
            /* get begin ptr */
            *va_arg(va, char **) = head->begin;
        } break;

        case ']': {
            /* get end ptr */
            *va_arg(va, char **) = head->end;
        } break;

        case '_': {
            /* skip to and over the end of list/object */
            while(head->sig != JSON_END) {
                json_skip(head);
            }
            json_next(head);
        } break;

        case '.': {
            /* skip one value */
            json_skip(head);
        } break;

        case 'm': {
            /* get mark */
            mark = va_arg(va, json_mark*);
            mark->begin = head->begin;
            mark->end = head->end;
            mark->ctx = head->ctx;
            mark->depth = head->stack.depth;
            mark->sig = head->sig;
            memcpy(&mark->valcache, &head->valcache, sizeof(union json_valcache));
        } break;

        case '\0': goto finish;

        default: break;

        }
        query++;
    }

error:
    rc = -1;

finish:
    va_end(va);
    return rc;
}




static void json_len(struct json_head * head) {
    if(head->sig != JSON_ARRAY) {

    }
}

static void json_restore(struct json_head *head, json_mark * mark) {
    head->begin = mark->begin;
    head->end = mark->end;
    head->stack.ptr = head->stack.begin + mark->depth;
    head->ctx = mark->ctx;
    head->sig = mark->sig;
    memcpy(&head->valcache, &mark->valcache, sizeof(union json_valcache));
}

#endif