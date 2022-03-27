#include "jsondb.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

#include "json.h"
#include "cjson.h"



#pragma region reference blocks

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
    size_t i = count;

    ensure_free_ref_count(count);

    /* split the current chain into requested chain and rest-chain */
    head = tail = free_refs.head;
    while(--i) {
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

#pragma endregion


#pragma region val, valp and ref operations

typedef unsigned short jsondb_size_t;

static void jsondb_val_incref(struct jsondb_val * val) {
    val->refct++;
}

static void jsondb_ref_set(jsondb_ref * ref, struct jsondb_val * val) {
    assert(val);
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

static jsondb_ref * jsondb_ref_last(jsondb_ref * ref) {
    if(ref)
        for(; ref->next; ref = ref->next);
    return ref;
}

static jsondb_ref * jsondb_ref_dup(jsondb_ref * ref) {
    jsondb_ref * new_ref = alloc_ref();
    jsondb_ref_set(new_ref, ref->val);
    return new_ref;
}

#pragma endregion


#pragma region value creation

static struct jsondb_val * jsondb_val_create(char * json) {
    struct jsondb_val * val;
    size_t size;
    cjson_ptr p;

    /* load stuff */
    p = cjson_load(json);
    size = cjson_measure(p) - p;

    /* allocate value */
    val = malloc(sizeof(struct jsondb_val) + size);

    /* set value fields */
    val->refct = 0;
    val->size = size;
    memcpy(JSONDB_CJSON_PTR(val), p, size);

    return val;
}


#pragma endregion


#pragma region set operations

static void jsondb_set_prepend(struct jsondb_set * set, jsondb_ref * ref) {
    if(!set->head) {
        set->head = set->tail = ref;
    } else {
        ref->next = set->head;
        set->head = ref;
    }
    set->size++;
}

static void jsondb_set_append(struct jsondb_set * set, jsondb_ref * ref) {
    if(!set->head) {
        set->head = set->tail = ref;
    } else {
        set->tail->next = ref;
        ref->next = set->head;
    }
    set->size++;
}

void jsondb_set_add(struct jsondb_set * set, char * json) {
    jsondb_ref *ref;
    struct jsondb_val *val;
    /* create the value */
    val = jsondb_val_create(json);
    /* allocate the reference */
    ref = alloc_ref();
    jsondb_ref_set(ref, val);
    /* and put in main db set */
    jsondb_set_prepend(set, ref);
}

void jsondb_set_add_set(struct jsondb_set *set, struct jsondb_set *add) {
    struct jsondb_set new_refs;
    new_refs = jsondb_set_dup(add);
    jsondb_set_join(set, &new_refs);
}

struct jsondb_set jsondb_set_single(char *json) {
    struct jsondb_set set;
    jsondb_set_add(&set, json);
    return set;
}

struct jsondb_set jsondb_set_dup(struct jsondb_set *set) {
    struct jsondb_set new = alloc_refs(set->size);
    jsondb_ref * a, * b;

    for(a = set->head, b = new.head; a; a = a->next, b = b->next) {
        jsondb_ref_set(b, a->val);
    }

    return new;
}

void jsondb_set_join(struct jsondb_set * into, struct jsondb_set * move) {
    if(move->size) {
        if(into->size) {
            move->tail->next = into->head;
            into->head = move->head;
            into->size += move->size;
        } else {
            *into = *move; /* lol */
        }
        memset(move, 0, sizeof(*move));
    }
}

static jsondb_ref * merge_lists(jsondb_ref * left, jsondb_ref * right) {
    jsondb_ref * head = NULL, * tail = NULL, * temp;

    if(!left) return right;
    if(!right) return left;
    
    /* select either the left or the right ref as next */
    while(left && right) {
        if(cjson_cmp(JSONDB_CJSON_PTR(left->val), JSONDB_CJSON_PTR(right->val)) < 0) {
            /* the left is smaller */
            temp = left;
            left = left->next;
        } else {
            /* the right is smaller or equal */
            temp = right;
            right = right->next;
        }

        temp->next = NULL;
        if(!tail) {
            head = tail = temp;
        } else {
            tail->next = temp;
            tail = temp;
        }
    }

    /* Either left or right may have elements left */
    /* knit the chains together */
    /* but at least one element is in head/tail */
    if(left) {
        tail->next = left;
    } else if(right) {
        tail->next = right;
    }

    return head;
}

void jsondb_set_sort(struct jsondb_set * set) {
    /* credit goes to wikipedia, just copy pasted and rewrote the pseudocode */
    jsondb_ref * array[32] = {0};
    jsondb_ref * result, * next;
    int i;

    if((set->flags & JSONDB_SET_SORTED) || set->size == 0) return;
    
    result = set->head;

    /* merge nodes into array */
    while(result) {
        next = result->next;
        result->next = NULL;
        for (i = 0; (i < 32) && array[i]; i++) {
            result = merge_lists(array[i], result);
            array[i] = NULL;
        }
        /* do not go past end of array */
        if(i == 32) i--;
        array[i] = result;
        result = next;
    }

    /* merge array into single list */
    result = NULL;
    for (i = 0; i < 32; i++)
        result = merge_lists(array[i], result);

    set->head = result;
    set->tail = jsondb_ref_last(result);
    set->flags |= JSONDB_SET_SORTED;
    /* size stays the same */
}

void jsondb_set_free(struct jsondb_set *set) {
    jsondb_ref * p = set->head;

    while(p) {
        jsondb_val_decref(p->val);
        p = p->next;
    }

    jsondb_set_join(&free_refs, set);
}

int jsondb_set_is_empty(struct jsondb_set *set) {
    return set->size == 0;
}

void jsondb_set_clear(struct jsondb_set *set) {
    jsondb_set_free(set);
}

struct jsondb_set jsondb_set_get(struct jsondb_set *set, char *path) {
    /* todo: duplicate code fragment */
    jsondb_ref *ref, *new_ref;
    cjson_ptr root, valp, valp_end;
    struct jsondb_set result = {0};
    struct jsondb_val *new_val;

    JSONDB_SET_FOREACH(set, ref) {
        root = JSONDB_CJSON_PTR(ref->val);
        if ((valp = cjson_get(root, path)) != NULL) {
            /* omg we found a value, thats amazing */
            /* alloc and copy over */
            valp_end = cjson_measure(valp);
            new_val = malloc(sizeof(struct jsondb_val) + (valp_end - valp));
            new_val->refct = 0;
            new_val->size = (valp_end - valp);
            memcpy(JSONDB_CJSON_PTR(new_val), valp, (valp_end - valp));

            /* new reference */
            new_ref = alloc_ref();
            jsondb_ref_set(new_ref, new_val);

            /* add reference */
            jsondb_set_prepend(&result, new_ref);
        }
    }

    return result;
}

struct jsondb_set jsondb_set_select_eq(struct jsondb_set * set, char * path, struct jsondb_set * choices) {
    /* todo: duplicate code fragment */
    jsondb_ref * set_ref, * new_ref, * choice_ref;
    cjson_ptr root, set_valp, choice_valp;
    struct jsondb_set result = {0};

    /* now check each of the references... */
    JSONDB_SET_FOREACH(set, set_ref) {
        root = JSONDB_CJSON_PTR(set_ref->val);
        if((set_valp = cjson_get(root, path)) == NULL) {
            /* has no attribute of the sort of _path_ */
            continue;
        }

        /* now try each of the choices */
        JSONDB_SET_FOREACH(choices, choice_ref) {
            choice_valp = JSONDB_CJSON_PTR(choice_ref->val);
            if (cjson_eq(set_valp, choice_valp)) {
                /* the value at the path and the cmp was equal, great */
                /* new reference */
                new_ref = alloc_ref();
                jsondb_ref_set(new_ref, set_ref->val);

                /* add reference */
                jsondb_set_prepend(&result, new_ref);
            }
        }
    }

    return result;
}

struct jsondb_set jsondb_set_select_cond(struct jsondb_set *set, jsondb_cond_func cond, void *custom_env) {
    jsondb_ref * ref;
    struct jsondb_set result = {0};

    /* now check each of the references... */
    JSONDB_SET_FOREACH(set, ref) {
        if(cond(ref, custom_env)) {
            /* then condition is true, add */
            jsondb_set_append(&result, jsondb_ref_dup(ref));
        }
    }

    /* thats it... */
    return result;
}

struct jsondb_set jsondb_set_union(struct jsondb_set *a, struct jsondb_set *b) {
    jsondb_ref * ahead, * bhead;
    struct jsondb_set result = {0};
    int cmp;
    
    /* just sort if not sorted yet */
    jsondb_set_sort(a);
    jsondb_set_sort(b);

    ahead = a->head;
    bhead = b->head;

    /* mostly copied and adapted from https://en.cppreference.com/w/cpp/algorithm/set_union */
    for (; ahead; ) {
        if (!bhead) {
            /* Finished range 2, include the rest of range 1: */
            for(; ahead; ahead = ahead->next) jsondb_set_append(&result, jsondb_ref_dup(ahead));
            goto finish;
        }

        /* compare em */
        cmp = cjson_cmp(JSONDB_CJSON_PTR(ahead->val), JSONDB_CJSON_PTR(bhead->val));

        /* todo to be optimized... */
        if(cmp < 0) {
            /* copy ahead over because it is smaller */
            jsondb_set_append(&result, jsondb_ref_dup(ahead));
            ahead = ahead->next;
        } else if (cmp > 0) {
            /* copy bhead over because it is smaller */
            jsondb_set_append(&result, jsondb_ref_dup(bhead));
            bhead = bhead->next;
        } else {
            /* copy one over, skip both (because equality) */
            jsondb_set_append(&result, jsondb_ref_dup(ahead));
            ahead = ahead->next;
            bhead = bhead->next;
        }
    }
    /* Finished range 1, include the rest of range 2: */
    for(; bhead; bhead = bhead->next) jsondb_set_append(&result, jsondb_ref_dup(bhead));

finish:
    result.flags |= JSONDB_SET_SORTED;
    return result;
}

struct jsondb_set jsondb_set_inter(struct jsondb_set *a, struct jsondb_set *b) {
    jsondb_ref * ahead, * bhead;
    struct jsondb_set result = {0};
    int cmp;
    
    /* just sort if not sorted yet */
    jsondb_set_sort(a);
    jsondb_set_sort(b);

    ahead = a->head;
    bhead = b->head;

    /* mostly copied and adapted from jsondb_set_union */
    while(ahead && bhead) {
        /* compare em */
        cmp = cjson_cmp(JSONDB_CJSON_PTR(ahead->val), JSONDB_CJSON_PTR(bhead->val));

        /* todo to be optimized... */
        if(cmp < 0) {
            /* skip a, because smaller */
            ahead = ahead->next;
        } else if (cmp > 0) {
            /* skip b, because bigger */
            bhead = bhead->next;
        } else {
            /* copy one over, skip both (because equality yay) */
            jsondb_set_append(&result, jsondb_ref_dup(ahead));
            ahead = ahead->next;
            bhead = bhead->next;
        }
    }

finish:
    result.flags |= JSONDB_SET_SORTED;
    return result;
}

struct jsondb_set jsondb_set_diff(struct jsondb_set *a, struct jsondb_set *b) {
    jsondb_ref * ahead, * bhead;
    struct jsondb_set result = {0};
    int cmp;
    
    /* just sort if not sorted yet */
    jsondb_set_sort(a);
    jsondb_set_sort(b);

    ahead = a->head;
    bhead = b->head;

    /* mostly copied and adapted jsondb_set_union, just like jsondb_set_inter */
    for (; ahead; ) {
        if (!bhead) {
            /* Finished range 2, include the rest of range 1: */
            for(; ahead; ahead = ahead->next) jsondb_set_append(&result, jsondb_ref_dup(ahead));
            goto finish;
        }

        /* compare em */
        cmp = cjson_cmp(JSONDB_CJSON_PTR(ahead->val), JSONDB_CJSON_PTR(bhead->val));

        /* todo to be optimized... */
        if(cmp < 0) {
            /* because a is smaller, it is getting included */
            jsondb_set_append(&result, jsondb_ref_dup(ahead));
            ahead = ahead->next;
        } else if (cmp > 0) {
            /* popping of b and not adding it, because b is the subtractor */
            bhead = bhead->next;
        } else {
            /* They're equal, and thats an exact subtraction situation, so dont add */
            ahead = ahead->next;
            bhead = bhead->next;
        }
    }

finish:
    result.flags |= JSONDB_SET_SORTED;
    return result;
}

#pragma endregion


#pragma region main db operations

void jsondb_add(char * json) {
    jsondb_set_add(&db_refs, json);
}

void jsondb_add_set(struct jsondb_set * add) {
    jsondb_set_add_set(&db_refs, add);
}

struct jsondb_set jsondb_get(char * path) {
    return jsondb_set_get(&db_refs, path);
}

struct jsondb_set jsondb_select_eq(char *path, struct jsondb_set * choices) {
    return jsondb_set_select_eq(&db_refs, path, choices);
}

struct jsondb_set jsondb_select_cond(jsondb_cond_func cond, void *custom_env) {
    return jsondb_set_select_cond(&db_refs, cond, custom_env);
}

void jsondb_join(struct jsondb_set *from) {
    jsondb_set_join(&db_refs, from);
}

void jsondb_init(void) {
    alloc_ref_block(1, &free_refs.size, &free_refs.head, &free_refs.tail);
}

void jsondb_deinit(void) {
    /* TODO */
    /* delete all values */
    /* delete all references (the blocks) */
}

#pragma endregion
