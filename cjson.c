#include "cjson.h"
#include "json.h"
#include "limits.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

typedef unsigned short cjson_size;

#define JSONDB_MAX_SIZE USHRT_MAX

char val_buf[64 * 1024];
cjson_ptr val_buf_end;
cjson_size val_off_buf[1024];

static void cjson_sort_obj_keys(cjson_ptr obj);

static void cjson_load_to_buffer(struct json_head * head, cjson_ptr * p, cjson_size * val_off_p) {
    enum json_sig sig;
    int i;
    int iv;
    float fv;
    struct { char * lo, * hi; } span;
    size_t strl;
    cjson_ptr val_begin;

    cjson_size * size_mark, * off_mark;
    size_t size;
    cjson_size * offs_mark;

#define WRITE(type, value)       \
    do {                         \
        *(type *)(*p) = (value); \
        *p += sizeof(type);      \
    } while (0)
#define SHIFT(begin, amount)                          \
    do {                                              \
        memmove((char*)(begin) + (amount), (char*)(begin), *p - (char*)(begin)); \
        *p += (amount);                               \
    } while (0)


    json_get(head, "t", &sig);

    switch(sig) {
        case JSON_NULL: {
            WRITE(char, CJSON_NULL);
            json_next(head);
        } break;

        case JSON_BOOL: {
            json_get(head, "i.", &iv);
            WRITE(char, iv ? CJSON_TRUE : CJSON_FALSE);
        } break;

        case JSON_NUM: {
            json_get(head, "[]", &span.lo, &span.hi);
            if(memchr(span.lo, '.', span.hi - span.lo) != NULL) {
                /* float because decimal point */
                json_get(head, "f.", &fv);
                WRITE(char, CJSON_F32);
                WRITE(float, fv);
            } else {
                /* int because no decimal point */
                json_get(head, "i.", &iv);
                WRITE(char, CJSON_I32);
                WRITE(int, iv);
            }
        } break;

        case JSON_STR: {
            json_get(head, "#", &strl);

            if(strl > JSONDB_MAX_SIZE) {
                abort();
            }

            WRITE(char, CJSON_STR);
            WRITE(cjson_size, strl);
            json_get(head, "s.", *p, (size_t)JSONDB_MAX_SIZE); /* TODO max string length */
            *p += strl;
        } break;

        case JSON_ARRAY: {
            /* TODO: json array size limits */
            WRITE(char, CJSON_ARRAY);
            size_mark = (cjson_size*)*p; *p += sizeof(cjson_size);
            size = 0;
            off_mark = (cjson_size *)*p;

            json_next(head);

            while(1) {
                json_get(head, "t", &sig);

                if(sig == JSON_END) {
                    break;
                } else {
                    *val_off_p++ = *p - (cjson_ptr)off_mark;
                    cjson_load_to_buffer(head, p, val_off_p);
                    size++;
                }
            }

            json_next(head);


            if (size > JSONDB_MAX_SIZE) {
                abort();
            }
            *size_mark = size;

            if(size > 0) {
                /* push everything right */
                SHIFT(off_mark, size * sizeof(cjson_size));

                /* copy over all the jump offsets */
                for ((off_mark += size - 1), val_off_p -= 1; off_mark > size_mark; off_mark--, val_off_p--) {
                    /* the recorded offset plus the space needed for the refs. */
                    /* so the offset os relative to _after_ the size */
                    *off_mark = *val_off_p + size * sizeof(cjson_size);
                }
            }

        } break;

        case JSON_OBJECT: {
            /* todo: key ordering, max entry count */
            val_begin = *p;

            WRITE(char, CJSON_OBJECT);
            size_mark = (cjson_size *)*p; *p += sizeof(cjson_size);
            size = 0;
            off_mark = (cjson_size *)*p;

            json_next(head);

            while(1) {
                json_get(head, "t", &sig);

                if(sig == JSON_END) {
                    break;
                } else {
                    *val_off_p++ = *p - (cjson_ptr)off_mark;
                    cjson_load_to_buffer(head, p, val_off_p);
                    cjson_load_to_buffer(head, p, val_off_p);
                    size++;
                }
            }

            json_next(head);


            if (size > JSONDB_MAX_SIZE) {
                abort();
            }
            *size_mark = size;

            if(size > 0) {
                /* push everything right */
                SHIFT(off_mark, size * sizeof(cjson_size));
                /* copy over all the jump offsets */
                for ((off_mark += size - 1), val_off_p -= 1; off_mark > size_mark; off_mark--, val_off_p--) {
                    *off_mark = *val_off_p + size * sizeof(cjson_size);
                }

                /* now sort the keys */
                cjson_sort_obj_keys(val_begin);
            }
        } break;

        default: break;
    }
}

static void cjson_sort_obj_keys(cjson_ptr obj) {
    /* holy fuck. this is gnome sort. TODO IMPROOOOOOOOOOOOOVE! */
    cjson_size size = *(cjson_size*)(obj+1);
    cjson_size * begin_ptr = (cjson_size*)(obj+1+sizeof(cjson_size)), * end_ptr = begin_ptr + (size-1), * ptr = begin_ptr;
    cjson_ptr a, b;
    cjson_size tmp;

    while(ptr < end_ptr) {
        a = ((cjson_ptr)begin_ptr) + ptr[0];
        b = ((cjson_ptr)begin_ptr) + ptr[1];
        if(cjson_cmp(a, b) > 0) {
            /* swap, because wrong order */
            tmp = ptr[0];
            ptr[0] = ptr[1];
            ptr[1] = tmp;
            /* do not walk over the lower end */
            if(ptr > begin_ptr) ptr--;
            else ptr++; /* swapping at the left end cannot disorder something even further left (because there is nothing), so move forward */
        } else {
            /* everything is in order */
            ptr++;
        }
    }
}


cjson_ptr cjson_load(char *json) {
    struct json_head head;
    enum json_ctx stack[32];
    cjson_ptr buf = val_buf;

    json_init(&head, json, stack, 32);

    cjson_load_to_buffer(&head, &buf, val_off_buf);
    val_buf_end = buf;

    return val_buf;
    
}

cjson_ptr cjson_load_safe(char * begin, char * end) {
    /* TODO: safe loading, with breaks */
    return cjson_load(begin);
}

cjson_ptr cjson_get(cjson_ptr ptr, char * key) {
    int search_index;
    char * key_end;
    size_t key_len;
    cjson_size size;
    int i;
    cjson_ptr entry;
    size_t entry_key_len;
    char * entry_key;


    /* check the "" case */
    if (*key == '\0') {
        return ptr;
    }
    assert(*key == '/');
    /* todo: add ~ stuff support and all the other stuff from RFC 6901 (JSON pointer) */
    key++;
    key_end = strchr(key, '/');
    if(!key_end) {
        key_end = key + strlen(key);
    }
    key_len = key_end - key;

    /* check for array */
    if(*((char*)ptr) == CJSON_ARRAY) {
        ptr++;
        
        search_index = atoi(key);
        size = *(cjson_size *)(ptr); ptr += sizeof(cjson_size);
        if (search_index >= size) {
            /* index out of bounds */
            return NULL;
        }

        ptr = ptr + ((cjson_size*)ptr)[search_index];
    }
    /* check for object */
    else if (*((char *)ptr) == CJSON_OBJECT) {
        ptr++;

        /* now check sequentially each entry for "key" */
        size = *(cjson_size *)(ptr); ptr += sizeof(cjson_size);
        for(i = 0; i < size; i++) {
            entry = ptr + ((cjson_size *)(ptr))[i];
            entry++; /* skip str marker byte */
            
            /* sizes must be equal */
            entry_key_len = *(cjson_size *)(entry); entry += sizeof(cjson_size); 
            if(entry_key_len != key_len) {
                continue;
            }

            /* their content as well */
            entry_key = entry;
            if(memcmp(key, entry_key, key_len) == 0) {
                /* found the key, jump to value */
                ptr = entry_key + key_len;
                goto next;
            }
        }

        /* No key found */
        return NULL;
    } else {
        /* we cannot subscript any other value */
        assert(0);
    }

next:
    return cjson_get(ptr, key_end);
}

cjson_ptr cjson_measure(cjson_ptr valp) {
    char type;
    cjson_size size;
    type = *(char *)(valp); valp++;

    switch(type) {
        case CJSON_TRUE:
        case CJSON_FALSE:
        case CJSON_NULL: break;
        case CJSON_F32:
        case CJSON_I32: {
            valp += 4;
        } break;
        case CJSON_STR: {
            valp += *(cjson_size*)(valp) + sizeof(cjson_size);
        } break;
        case CJSON_ARRAY: {
            size = *(cjson_size*)(valp); valp += sizeof(cjson_size);
            valp = size == 0? valp : cjson_measure(valp + ((cjson_size*)(valp))[size-1]);
        } break;
        case CJSON_OBJECT: {
            size = *(cjson_size *)(valp); valp += sizeof(cjson_size);
            /* crazy shit's happenin here */
            valp = size == 0? valp : cjson_measure(cjson_measure(valp + ((cjson_size *)(valp))[size - 1]));
        } break;
    }

    return valp;
}

int cjson_cmp(cjson_ptr p1, cjson_ptr p2) {
    char type1 = *(char*)(p1), type2 = *(char*)(p2);
    float fd;
    size_t p1len, p2len, plen;
    int cmp;
    int i;
    cjson_ptr temp1, temp2;
    size_t key_size;

    if(type1 != type2) {
        return type1 - type2;
    }

    switch(type1) {
        case CJSON_NULL:
        case CJSON_TRUE:
        case CJSON_FALSE: return 0;
        case CJSON_I32: return *(int *)(p1 + 1) - *(int *)(p2 + 1);
        case CJSON_F32: {
            fd = *(float*)(p1 + 1) - *(float*)(p2 + 1);
            return fd == 0.f? 0 : fd > 0.f? 1 : -1;
        }
        case CJSON_STR: {
            p1++, p2++;
            p1len = *(cjson_size *)(p1);
            p2len = *(cjson_size *)(p2);
            plen = p1len > p2len? p2len : p1len;
            cmp = memcmp(p1 + sizeof(cjson_size), p2 + sizeof(cjson_size), plen);
            return cmp? cmp : p1len - p2len;
        }
        case CJSON_ARRAY: {
            p1++, p2++;
            p1len = *(cjson_size *)(p1); p1 += sizeof(cjson_size);
            p2len = *(cjson_size *)(p2); p2 += sizeof(cjson_size);
            plen = p1len > p2len ? p2len : p1len;
            for(i = 0; i < plen; i++) {
                /* check each of their elements */
                cmp = cjson_cmp(p1 + ((cjson_size *)p1)[i], p2 + ((cjson_size *)p2)[i]);
                if(cmp) return cmp;
            }
            /* for the minimal length of both, they're equal - now the length decides */
            return p1len - p2len;
        }
        case CJSON_OBJECT: {
            /* Because the keys are sorted, we compare sequentially the entries (key - value) */
            p1++, p2++;
            p1len = *(cjson_size *)(p1); p1 += sizeof(cjson_size);
            p2len = *(cjson_size *)(p2); p2 += sizeof(cjson_size);
            plen = p1len > p2len ? p2len : p1len;
            for(i = 0; i < plen; i++) {
                /* check each of their entries. */
                /* check the key: */
                temp1 = p1 + ((cjson_size *)p1)[i];
                temp2 = p2 + ((cjson_size *)p2)[i];
                cmp = cjson_cmp(temp1, temp2);
                if(cmp) return cmp;
                /* afterward, check the value: TODO: temp1 and temp2 at this point have equal length,  */
                key_size = cjson_measure(temp1) - temp1;
                temp1 = temp1 + key_size;
                temp2 = temp2 + key_size;
                cmp = cjson_cmp(temp1, temp2);
                if(cmp) return cmp;
            }
            /* for the minimal length of both, they're equal - now the length decides */
            return p1len - p2len;
        }
    }
    assert(0);
    return 0;
}

int cjson_eq(cjson_ptr p1, cjson_ptr p2) {
    /* todo: implement custom version here */
    return cjson_cmp(p1, p2) == 0;
}

size_t cjson_count(cjson_ptr ptr) {
    switch(*ptr++) {
        case CJSON_STR:
        case CJSON_OBJECT:
        case CJSON_ARRAY: {
            return *(cjson_size*)ptr; 
        };
        default: { assert(0); return 0; }
    }
}

int cjson_as_int(cjson_ptr ptr) {
    switch(*ptr) {
        case CJSON_NULL: return 0;
        case CJSON_TRUE: return 1;
        case CJSON_FALSE: return 0;
        case CJSON_I32: return *(int*)(ptr+1);
        case CJSON_F32: return (int)*(float*)(ptr+1);
        default: { assert(0); return 0; }
    }
}

float cjson_as_float(cjson_ptr ptr) {
    switch(*ptr) {
        case CJSON_NULL: return 0.0f;
        case CJSON_TRUE: return 1.0f;
        case CJSON_FALSE: return 0.0f;
        case CJSON_I32: return (float)*(int*)(ptr+1);
        case CJSON_F32: return *(float*)(ptr+1);
        default: { assert(0); return 0; }
    }
}

void * cjson_data(cjson_ptr ptr) {
    switch(*ptr) {
        case CJSON_NULL: 
        case CJSON_TRUE: 
        case CJSON_FALSE: return NULL;
        case CJSON_I32: return ptr +1;
        case CJSON_F32: return ptr +1;
        case CJSON_STR: return ptr +1+sizeof(cjson_size);
        default: { assert(0); return 0; }
    }
}

cjson_ptr cjson_array_get(cjson_ptr arr, size_t index) {
    cjson_size * offs;
    assert(*arr == CJSON_ARRAY);
    /* also asserting count > 0 and index < coutn */
    offs = (cjson_size*)(arr+1+sizeof(cjson_size));
    return (cjson_ptr)(offs) + offs[index];
}

static cjson_ptr cjson_obj_entry(cjson_ptr obj, char * key) {
    cjson_size * off_ptr;
    cjson_ptr entry;
    cjson_size size;
    int i;
    size_t key_len = strlen(key);

    size = *(cjson_size*)(obj+1);
    off_ptr = (cjson_size*)(obj+1+sizeof(cjson_size));
    /* todo: upgrade to binary search */
    for (i = 0; i < size; i++) {
        entry = (cjson_ptr)off_ptr + off_ptr[i];
        /* todo: has the potential for Errors */
        if(key_len == cjson_count(entry) && memcmp(key, cjson_data(entry), key_len) == 0) {
            return cjson_measure(entry); /* skips the string and lands on the value */
        }
    }
    return NULL;
}

cjson_ptr cjson_obj_get(cjson_ptr obj, char * key) {
    cjson_ptr entry = cjson_obj_entry(obj, key);
    if(entry) return cjson_measure(entry);
    else return NULL;
}

int cjson_obj_has_key(cjson_ptr obj, char *key) {
    return cjson_obj_entry(obj, key) != NULL;
}