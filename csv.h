#ifndef _CSV_H_
#define _CSV_H_

#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>


enum csv_sig {
    CSV_VAL,
    CSV_END,
    CSV_ERROR,
    CSV_INITIAL,
    CSV_HALT,
};


/*
Grammar:

initial -> record*

record -> VAL* END


*/

struct csv_head {
    char * begin, * end;
    enum csv_sig sig;
    size_t strlen;

    char sep;
};

void csv_init(struct csv_head * head, char * doc, char sep);
void csv_next(struct csv_head * head);


void csv_init(struct csv_head * head, char * doc, char sep) {
    head->end = doc;
    head->sig = CSV_INITIAL;
    head->sep = sep;

    csv_next(head);
}

char * csv_bump(char * str) {
    while(isspace(*str)) str++;
    return str;
}

void csv_next(struct csv_head * head) {
    char * p = head->end;
    char prev_char = -1;
    
    head->begin = p;

    switch(head->sig) {
    case CSV_ERROR: goto error;
    case CSV_HALT: goto finish;
    default: break;
    }

    /* if previously a val was read, skip a separator */
    if(head->sig == CSV_VAL && *p == head->sep) {
        p++;
    }

    /* check for document end */
    if(*p == '\0') {
        head->sig = head->sig == CSV_END? CSV_HALT : CSV_END;
        goto finish;
    } else /* check for record end */ if((p[0] == '\n' || p[0] == '\r')) {
        /* skip line endings */
        if(p[0] == '\r' && p[1] == '\n') {
            p += 2;
        } else {
            p++;
        }
        head->end = p;
        head->sig = CSV_END;
        goto finish;
    } 

    if(*p == '"') {
        /* read a quoted value */
        p++;

        while(1) {
            if(p[0] == '"') {
                if(p[1] == '"') {
                    p += 2;
                } else {
                    p++;
                    break;
                }
            } else {
                p++;
                head->strlen++;
            }
        }

        p++;
    } else {
        /* read a normal value */
        while(*p != head->sep && *p != '\n' && *p != '\r' && *p != '\0') {
            p++;
            head->strlen++;
        }
    }

    head->end = p;
    head->sig = CSV_VAL;

error:
finish:
    head->end = p;
    return;
}

int csv(struct csv_head * head, const char * query, ...) {
    va_list va;
    int rc = -1;

    float floatv;
    int intv;
    struct {
        char * buf;
        size_t cap;
    } strv;

    char * p;

    va_start(va, query);

    while(*query) {
        switch(*query) {

        case '[': {
            /* get val beginning */
            *va_arg(va, char **) = head->begin;
        } break;

        case ']': {
            /* get val end */
            *va_arg(va, char **) = head->end;
        } break;

        case 'f': {
            if(head->sig != CSV_VAL) goto error;
            /* parse as  */
            floatv = (float)strtod(head->begin, NULL);
            *va_arg(va, float *) = floatv;
        } break;

        case 'i': {
            if(head->sig != CSV_VAL) goto error;
            /* parse as integer */
            intv = strtol(head->begin, NULL, 10);
            *va_arg(va, int *) = intv;
        } break;

        case 's': {
            if(head->sig != CSV_VAL) goto error;

            /* load into buffer */
            strv.buf = va_arg(va, char *);
            strv.cap = va_arg(va, size_t);

            if(*head->begin != '"') {
                /* not quoted */
                if(head->strlen > strv.cap) {
                    memcpy(strv.buf, head->begin, strv.cap);
                } else {
                    memcpy(strv.buf, head->begin, head->strlen);
                    strv.buf[head->strlen] = '\0';
                }
            } else {
                /* indeed quoted */
                p = head->begin + 1;
                while(p < head->end-1 && strv.cap > 0) {
                    if(*p == '"') {
                        /* thats an escape */
                        *strv.buf++ = '"';
                        p += 2;
                    } else {
                        *strv.buf++ = *p++;
                    }
                } 
            }
        } break;

        case '_': {
            /* skip until next record */
            while(head->sig == CSV_VAL)
                csv_next(head);
        }

        case '.': {
            /* skip one value */
            csv_next(head); 
        } break;

        }

        query++;
    }

finish:
    rc = 0;
error:
    va_end(va);
    return rc;
}


#endif