#ifndef _JSONDB_H_
#define _JSONDB_H_

#include <stdlib.h>
#include "cjson.h"


/* Different JSONDB-Internal Types */
enum jsondb_types {
    JSONDB_TYPE_NULL,
    JSONDB_TYPE_FALSE,
    JSONDB_TYPE_TRUE,
    JSONDB_TYPE_I32,
    JSONDB_TYPE_F32,
    JSONDB_TYPE_STR,
    JSONDB_TYPE_ARRAY,
    JSONDB_TYPE_OBJECT,
};


struct jsondb_val {
    int refct;
    unsigned size;
};

#define JSONDB_CJSON_PTR(val) ((cjson_ptr)(val) + sizeof(struct jsondb_val))


typedef struct jsondb_ref {
    struct jsondb_val * val;
    struct jsondb_ref * next;
} jsondb_ref; 


struct jsondb_set {
    jsondb_ref * head, * tail;
    size_t size;
    unsigned flags;
};

#define JSONDB_SET_SORTED (1 << 0)
#define JSONDB_SET_FROZEN (1 << 1)

#define JSONDB_SET_EMPTY {0}

#define JSONDB_SET_FOREACH(_setp, _refp) for ((_refp) = (_setp)->head; (_refp); (_refp) = (_refp)->next)





typedef int (*jsondb_cond_func)(jsondb_ref * ref, void * custom_env);



void                jsondb_init             (void);
void                jsondb_deinit           (void);

void                jsondb_add              (char * json);
void                jsondb_add_cjson        (cjson_ptr json);
void                jsondb_add_set          (struct jsondb_set * add);
struct jsondb_set   jsondb_get              (char * path);
struct jsondb_set   jsondb_select_eq        (char * path, struct jsondb_set * choices);
struct jsondb_set   jsondb_select_cond      (jsondb_cond_func cond, void * custom_env);
/* void                jsondb_diff             (struct jsondb_set * sub); */
void                jsondb_join             (struct jsondb_set * from);

struct jsondb_set   jsondb_set_single       (char * json);
void                jsondb_set_add          (struct jsondb_set * set, char * json);
void                jsondb_set_add_set      (struct jsondb_set * set, struct jsondb_set * add);
void                jsondb_set_add_cjson    (struct jsondb_set * set, cjson_ptr json);
struct jsondb_set   jsondb_set_dup          (struct jsondb_set * set);
void                jsondb_set_join         (struct jsondb_set * into, struct jsondb_set * from);
struct jsondb_set   jsondb_set_get          (struct jsondb_set * set, char * path);
struct jsondb_set   jsondb_set_select_eq    (struct jsondb_set * set, char * path, struct jsondb_set * choices);
struct jsondb_set   jsondb_set_select_cond  (struct jsondb_set * set, jsondb_cond_func cond, void * custom_env);
struct jsondb_set   jsondb_set_union        (struct jsondb_set * a, struct jsondb_set * b);
struct jsondb_set   jsondb_set_inter        (struct jsondb_set * a, struct jsondb_set * b);
struct jsondb_set   jsondb_set_diff         (struct jsondb_set * set, struct jsondb_set * sub);
void                jsondb_set_sort         (struct jsondb_set * set);
int                 jsondb_set_is_empty     (struct jsondb_set * set);
void                jsondb_set_clear        (struct jsondb_set * set);
void                jsondb_set_free         (struct jsondb_set * set);

#endif