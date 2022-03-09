#include "jsondb.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "json.h"



/* REFERENCE BLOCKS */

#define JSONDB_REFBLOCK_CT 1024


struct {
    struct jsondb_refblock {
        jsondb_ref * block;
        size_t size;
        struct jsondb_refblock * next;
    } * blockchain; /* lol */
    size_t total;
} ref_info = {0};


struct jsondb_set free_refs = {0};

struct jsondb_set db_refs = {0};



static void alloc_ref_block(size_t min_count, size_t * alloc_count, jsondb_ref ** head, jsondb_ref ** tail) {
    size_t aligned_count;
    jsondb_ref * refs;
    size_t i;
    struct jsondb_refblock * refblock;

    /* align and alloc */
    aligned_count = ((min_count + JSONDB_REFBLOCK_CT - 1) / JSONDB_REFBLOCK_CT) * JSONDB_REFBLOCK_CT;
    refs = malloc(sizeof(jsondb_ref) * aligned_count);

    /* link em up */
    for(i = 0; i < aligned_count - 1; i++) {
        refs[i].next = refs + i + 1;
    }
    /* last one points to NULL */
    refs[aligned_count-1].next = NULL;

    refblock = malloc(sizeof(*refblock));
    refblock->block = refs;
    refblock->size = aligned_count;
    refblock->next = ref_info.blockchain;
    ref_info.blockchain = refblock;
    ref_info.total += aligned_count;

    /* return values */
    *alloc_count = aligned_count;
    *head = refs;
    *tail = refs + aligned_count - 1;
}

static void ensure_free_ref_count(size_t count) {
    jsondb_ref *block_head, *block_tail;
    size_t alloc_count;
    
    if (free_refs.size <= count) {
        /* allocate enough refs */
        alloc_ref_block(count, &alloc_count, &block_head, &block_tail);
        /* append em to the free set */
        free_refs.tail->next = block_head;
        free_refs.tail = block_tail;
        free_refs.size += alloc_count;
    }
}


static struct jsondb_set alloc_refs(size_t count) {
    jsondb_ref * head, * tail;
    struct jsondb_set set;

    ensure_free_ref_count(count);

    /* split the current chain into requested chain and rest-chain */
    head = tail = free_refs.head;
    while(--count) {
        tail = tail->next;
    }

    /* remove from free_refs */
    free_refs.head = tail->next;
    tail->next = NULL; /* mark as end */

    /* init new set */
    set.head = head;
    set.tail = tail;
    set.size = count;

    return set;
}

static jsondb_ref * alloc_ref(void) {
    jsondb_ref * ref;

    ensure_free_ref_count(1);

    ref = free_refs.head;
    free_refs.head = ref->next;
    ref->next = NULL;
    free_refs.size--;

    return ref;
}


/* JSONDB VALUE OPERATIONS */

typedef unsigned short jsondb_size_t;

static void jsondb_val_incref(struct jsondb_val * val) {
    val->refct++;
}

static void jsondb_ref_set(jsondb_ref * ref, struct jsondb_val * val) {
    ref->val = val;
    val->refct++;
}

static void jsondb_val_decref(struct jsondb_val *val) {
    val->refct--;
    if(val->refct <= 0) {
        /* TODO: deallocate */
    }
}

static void jsondb_ref_clear(jsondb_ref * ref) {
    ref->val->refct--;
    if(ref->val->refct <= 0) {
        /* TODO: deallocate value */
    }
    ref->val = NULL;
}


static jsondb_valp jsondb_valp_get(jsondb_valp valp, char * key) {
    ssize_t search_index;
    char * key_end;
    size_t key_len;
    jsondb_size_t size;
    int i;
    jsondb_valp entry;
    size_t entry_key_len;
    char * entry_key;


    /* check the "" case */
    if (*key == '\0') {
        return valp;
    } else if(*key != '/') {
        /* no / found, thats bad */
        /* TODO: replace aborts */
        abort();
    } else {
        /* todo: add ~ stuff support and all the other stuff from RFC 6901 (JSON pointer) */
        key++;
        key_end = strchr(key, '/');
        if(!key_end) {
            key_end = key + strlen(key);
        }
        key_len = key_end - key;
    }

    /* check for array */
    if(*((char*)valp) == JSONDB_TYPE_ARRAY) {
        valp++;
        
        search_index = atoi(key);
        size = *(jsondb_size_t *)(valp); valp += sizeof(jsondb_size_t);
        if (search_index >= size) {
            /* index out of bounds */
            return NULL;
        }

        valp = valp + ((jsondb_size_t*)valp)[search_index];
    }
    /* check for object */
    else if (*((char *)valp) == JSONDB_TYPE_OBJECT) {
        valp++;

        /* now check sequentially each entry for "key" */
        size = *(jsondb_size_t *)(valp); valp += sizeof(jsondb_size_t);
        for(i = 0; i < size; i++) {
            entry = valp + ((jsondb_size_t *)(valp))[i];
            entry++; /* skip str marker byte */
            
            /* sizes must be equal */
            entry_key_len = *(jsondb_size_t *)(entry); entry += sizeof(jsondb_size_t); 
            if(entry_key_len != key_len) {
                continue;
            }

            /* their content as well */
            entry_key = entry;
            if(memcmp(key, entry_key, key_len) == 0) {
                /* found the key, jump to value */
                valp = entry_key + key_len;
                goto next;
            }
        }

        /* No key found */
        return NULL;
    } else {
        /* we cannot subscript any other value */
        abort();
    }

next:
    return jsondb_valp_get(valp, key_end);
}

static jsondb_valp jsondb_valp_measure(jsondb_valp valp) {
    char type = *(char *)(valp); valp++;
    jsondb_size_t size;

    switch(type) {
        case JSONDB_TYPE_TRUE:
        case JSONDB_TYPE_FALSE:
        case JSONDB_TYPE_NULL: break;
        case JSONDB_TYPE_F32:
        case JSONDB_TYPE_I32: {
            valp += 4;
        } break;
        case JSONDB_TYPE_STR: {
            valp += *(jsondb_size_t*)(valp) + sizeof(jsondb_size_t);
        } break;
        case JSONDB_TYPE_ARRAY: {
            size = *(jsondb_size_t*)(valp); valp += sizeof(jsondb_size_t);
            valp = size == 0? valp : jsondb_valp_measure(valp + ((jsondb_size_t*)(valp))[size-1]);
        } break;
        case JSONDB_TYPE_OBJECT: {
            size = *(jsondb_size_t *)(valp); valp += sizeof(jsondb_size_t);
            /* crazy shit's happenin here */
            valp = size == 0? valp : jsondb_valp_measure(jsondb_valp_measure(valp + ((jsondb_size_t *)(valp))[size - 1]));
        } break;
    }

    return valp;
}

/* SET OPERATIONS */

static void jsondb_set_prepend(struct jsondb_set * set, jsondb_ref * ref) {
    if(!set->head) {
        set->head = set->tail = ref;
    } else {
        ref->next = set->head;
        set->head = ref;
    }
    set->size++;
}

static void jsondb_set_move_into(struct jsondb_set * into, struct jsondb_set * move) {
    move->tail->next = into->head;
    into->head = move->head;
    memset(move, 0, sizeof(*move));
}

void jsondb_set_free(struct jsondb_set *set) {
    jsondb_ref * p = set->head;

    while(p) {
        jsondb_val_decref(p->val);
        p = p->next;
    }

    jsondb_set_move_into(&free_refs, set);
}

/* JSON VALUE CREATION */

#define JSONDB_MAX_SIZE USHRT_MAX

char val_buf[64 * 1024];
jsondb_size_t val_off_buf[1024];

static void load_json_to_buf(struct json_head * head, jsondb_valp * p, jsondb_size_t * val_off_p) {
    enum json_sig sig;
    int i;
    int iv;
    float fv;
    struct { char * lo, * hi; } span;
    size_t strl;

    jsondb_size_t * size_mark, * off_mark;
    size_t size;
    jsondb_size_t * offs_mark;

#define WRITE(type, value)       \
    do {                         \
        *(type *)(*p) = (value); \
        *p += sizeof(type);      \
    } while (0)
#define SHIFT(begin, amount)                          \
    do {                                              \
        memmove((void*)(begin) + (amount), (void*)(begin), *p - (void*)(begin)); \
        *p += (amount);                               \
    } while (0)

    json_get(head, "t", &sig);

    switch(sig) {
        case JSON_NULL: {
            WRITE(char, JSONDB_TYPE_NULL);
            json_next(head);
        } break;

        case JSON_BOOL: {
            json_get(head, "i.", &iv);
            WRITE(char, iv ? JSONDB_TYPE_TRUE : JSONDB_TYPE_FALSE);
        } break;

        case JSON_NUM: {
            json_get(head, "[]", &span.lo, &span.hi);
            if(memchr(span.lo, '.', span.hi - span.lo) != NULL) {
                /* float because decimal point */
                json_get(head, "f.", &fv);
                WRITE(char, JSONDB_TYPE_F32);
                WRITE(float, fv);
            } else {
                /* int because no decimal point */
                json_get(head, "i.", &iv);
                WRITE(char, JSONDB_TYPE_I32);
                WRITE(int, iv);
            }
        } break;

        case JSON_STR: {
            json_get(head, "#", &strl);

            if(strl > JSONDB_MAX_SIZE) {
                abort();
            }

            WRITE(char, JSONDB_TYPE_STR);
            WRITE(jsondb_size_t, strl);
            json_get(head, "s.", *p, (size_t)JSONDB_MAX_SIZE); /* TODO max string length */
            *p += strl;
        } break;

        case JSON_ARRAY: {
            /* TODO: json array size limits */
            WRITE(char, JSONDB_TYPE_ARRAY);
            size_mark = *p; *p += sizeof(jsondb_size_t);
            size = 0;
            off_mark = *p;

            json_next(head);

            while(1) {
                json_get(head, "t", &sig);

                if(sig == JSON_END) {
                    break;
                } else {
                    *val_off_p++ = *p - (void *)off_mark;
                    load_json_to_buf(head, p, val_off_p);
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
                SHIFT(off_mark, size * sizeof(jsondb_size_t));

                /* copy over all the jump offsets */
                for ((off_mark += size - 1), val_off_p -= 1; off_mark > size_mark; off_mark--, val_off_p--) {
                    /* the recorded offset plus the space needed for the refs. */
                    /* so the offset os relative to _after_ the size */
                    *off_mark = *val_off_p + size * sizeof(jsondb_size_t);
                }
            }

        } break;

        case JSON_OBJECT: {
            /* todo: key ordering, max entry count */
            WRITE(char, JSONDB_TYPE_OBJECT);
            size_mark = *p; *p += sizeof(jsondb_size_t);
            size = 0;
            off_mark = *p;

            json_next(head);

            while(1) {
                json_get(head, "t", &sig);

                if(sig == JSON_END) {
                    break;
                } else {
                    *val_off_p++ = *p - (void*)off_mark;
                    load_json_to_buf(head, p, val_off_p);
                    load_json_to_buf(head, p, val_off_p);
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
                SHIFT(off_mark, size * sizeof(jsondb_size_t));
                /* copy over all the jump offsets */
                for ((off_mark += size - 1), val_off_p -= 1; off_mark > size_mark; off_mark--, val_off_p--) {
                    *off_mark = *val_off_p + size * sizeof(jsondb_size_t);
                }
            }
        } break;

        default: break;
    }
}


/* API OPERATIONS */

void jsondb_insert(char * json, char * json_end) {
    struct json_head head;
    struct jsondb_val * val;
    jsondb_ref * ref;
    size_t size;
    jsondb_valp p = val_buf;

    /* load json value to buffer */
    json_init(&head, json, NULL, 0);
    load_json_to_buf(&head, &p, val_off_buf);

    /* allocate value */
    size = p - (void*)val_buf;
    val = malloc(sizeof(struct jsondb_val) + size);

    /* set value fields */
    val->refct = 0;
    val->size = size;
    memcpy(JSONDB_VALP(val), val_buf, size);

    /* allocate the reference */
    ref = alloc_ref();
    jsondb_ref_set(ref, val);
    /* and put in main db set */
    jsondb_set_prepend(&db_refs, ref);
}

struct jsondb_set jsondb_select(char * path) {
    jsondb_ref * ref = db_refs.head, * new_ref;
    jsondb_valp root, valp, valp_end;
    struct jsondb_set result = {0};
    struct jsondb_val * new_val;

    while(ref) {
        root = JSONDB_VALP(ref->val);
        if((valp = jsondb_valp_get(root, path)) != NULL) {
            /* omg we found a value, thats amazing */
            /* alloc and copy over */
            valp_end = jsondb_valp_measure(valp);
            new_val = malloc(sizeof(struct jsondb_val) + (valp_end - valp));
            new_val->refct = 0;
            new_val->size = (valp_end - valp);
            memcpy(JSONDB_VALP(new_val), valp, (valp_end - valp));

            /* new reference */
            new_ref = alloc_ref();
            jsondb_ref_set(new_ref, new_val);

            /* add reference */
            jsondb_set_prepend(&result, new_ref);
        }
        ref = ref->next;
    }

    return result;
}



void jsondb_init(void) {
    alloc_ref_block(1, &free_refs.size, &free_refs.head, &free_refs.tail);
}

void jsondb_deinit(void) {
    /* TODO */
    /* delete all values */
    /* delete all references (the blocks) */
}

