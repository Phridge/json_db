#ifndef _JSONDB_H_
#define _JSONDB_H_


/* Different JSONDB-Internal Types */
enum jsondb_types {
    JSONDB_TYPE_OBJECT,
    JSONDB_TYPE_ARRAY,
    JSONDB_TYPE_STR,
    JSONDB_TYPE_F32,
    JSONDB_TYPE_I32,
    JSONDB_TYPE_TRUE,
    JSONDB_TYPE_FALSE,
    JSONDB_TYPE_NULL,
};

typedef unsigned char jsondb_tword;

typedef unsigned int jsondb_vword;


struct jsondb_val {
    int refct;
    jsondb_tword * tvec;
    jsondb_vword * wvec;
};

typedef struct jsondb_ref {
    struct jsondb_val * val;
    struct jsondb_next * next;
} jsondb_ref; 

void jsondb_init(void);
void jsondb_deinit(void);

void jsondb_insert(char * json, char * json_end);

#endif