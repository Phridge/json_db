#include "jsondb.h"
#include <stdlib.h>
#include <string.h>

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
        refs[i].val = NULL;
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


/* SET OPERATIONS */

static void jsondb_set_prepend(struct jsondb_set * set, jsondb_ref * ref) {
    if(!set->head) {
        set->head = set->tail = ref;
    } else {
        ref->next = set->head;
        set->head = ref;
    }
    ref->val->refct++;
    set->size++;
}


/* JSON VALUE CREATION */

jsondb_tword __tbuf_arr[4096];
jsondb_vword __vbuf_arr[4096];

struct {
    jsondb_tword * tbuf, * tptr;
    jsondb_vword * vbuf, * vptr;
} val_buf = {
    __tbuf_arr, __tbuf_arr,
    __vbuf_arr, __vbuf_arr,
};


size_t upscale(size_t num, size_t blksize) {
    return (num + blksize - 1) / blksize;
}


static void val_buf_write_size(size_t size) {

}

static void load_json_to_buf(struct json_head * head) {
    enum json_sig sig;

    int iv;
    float fv;
    struct { char * lo, * hi; } span;
    size_t strl, vstrl;

    jsondb_tword * mark;
    size_t size;

    json_get(head, "t", &sig);

    switch(sig) {
        case JSON_NULL: {
            *val_buf.tptr++ = JSONDB_TYPE_NULL;
            json_next(head);
        } break;

        case JSON_BOOL: {
            json_get(head, "i.", &iv);
            *val_buf.tptr++ = iv? JSONDB_TYPE_TRUE : JSONDB_TYPE_FALSE;
        } break;

        case JSON_NUM: {
            json_get(head, "[]", &span.lo, &span.hi);
            if(memchr(span.lo, '.', span.hi - span.lo) != NULL) {
                /* float because decimal point */
                json_get(head, "f.", &fv);
                *(int* )(val_buf.vptr)++ = iv;
            } else {
                /* int because no decimal point */
                json_get(head, "i.", &iv);
                *(float *)(val_buf.vptr)++ = fv;
            }
        } break;

        case JSON_STR: {
            json_get(head, "#", &strl);
            vstrl = upscale(strl, sizeof(jsondb_vword));

            if(vstrl > 255) {
                abort();
            }

            if(strl > 0) {
                /* Black magic */
                val_buf.vptr[vstrl-1] = ~(jsondb_vword)0;
            }
            
            json_get(head, "s.", val_buf.vptr, 255 * sizeof(jsondb_vword)); /* TODO max string length */

            val_buf.vptr += vstrl;
            *val_buf.tptr++ = JSONDB_TYPE_STR;
            *val_buf.tptr++ = vstrl;
        } break;

        case JSON_ARRAY: {
            /* TODO: json array size limits */
            *val_buf.tptr++ = JSONDB_TYPE_ARRAY;
            mark = val_buf.tptr++;
            size = 0;

            json_next(head);

            while(1) {
                json_get(head, "t", &sig);

                if(sig == JSON_END) {
                    break;
                } else {
                    load_json_to_buf(head);
                    size++;
                }
            }

            if (size > 255) {
                abort();
            }
            *mark = size;
            json_next(head);
        } break;

        case JSON_OBJECT: {
            /* todo: key ordering */
            *val_buf.tptr++ = JSONDB_TYPE_OBJECT;
            mark = val_buf.tptr++;
            size = 0;

            json_next(head);

            while(1) {
                json_get(head, "t", &sig);

                if(sig == JSON_END) {
                    break;
                } else {
                    /* load the string */
                    load_json_to_buf(head); 
                    /* load the value */
                    load_json_to_buf(head);
                    size++;
                }
            }

            if (size > 255) {
                abort();
            }
            *mark = size;
            json_next(head);
        } break;

        case JSON_ERROR: {
            /* todo: handle error */
        } break;
    }
}




void jsondb_insert(char * json, char * json_end) {
    struct json_head head;
    struct jsondb_val * val;
    struct jsondb_ref * ref;
    size_t aligned_tvec_size, total_size;

    json_init(&head, json, NULL, 0);

    load_json_to_buf(&head);

    aligned_tvec_size = upscale(val_buf.tptr - val_buf.tbuf, sizeof(jsondb_vword)) * sizeof(jsondb_vword);
    total_size = sizeof(struct jsondb_val) + aligned_tvec_size + (val_buf.vptr - val_buf.vbuf) * sizeof(jsondb_vword);

    val = malloc(total_size);

    val->refct = 0;
    val->size = total_size;
    val->tvec = (void*)(val) + sizeof(struct jsondb_val);
    val->vvec = (void*)(val->tvec) + aligned_tvec_size;
    memcpy(val->tvec, val_buf.tptr, (val_buf.tptr - val_buf.tbuf) * sizeof(jsondb_tword));
    memcpy(val->vvec, val_buf.vptr, (val_buf.vptr - val_buf.vbuf) * sizeof(jsondb_vword));

    val_buf.tptr = val_buf.tbuf;
    val_buf.vptr = val_buf.vbuf;

    ref = alloc_ref();
    ref->val = val;

    jsondb_set_prepend(&db_refs, ref);
}

void jsondb_init(void) {
    alloc_ref_block(1, &free_refs.size, &free_refs.head, &free_refs.tail);
}

void jsondb_deinit(void) {
    /* TODO */
    /* delete all values */
    /* delete all references (the blocks) */
}

