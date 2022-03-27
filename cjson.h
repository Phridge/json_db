#ifndef _CJSON_H_
#define _CJSON_H_

#include <stdlib.h>

/* Compiled/Compressed JSON. Uses global buffers, so no multithreading. 
 * Uses json.h under the hood. */

typedef char * cjson_ptr;

enum cjson_type {
    CJSON_NULL,
    CJSON_FALSE,
    CJSON_TRUE,
    CJSON_I32,
    CJSON_F32,
    CJSON_STR,
    CJSON_ARRAY,
    CJSON_OBJECT,
};

cjson_ptr       cjson_load          (char * json);
cjson_ptr       cjson_load_safe     (char * begin, char * end);
cjson_ptr       cjson_get           (cjson_ptr ptr, char * path);
cjson_ptr       cjson_measure       (cjson_ptr ptr);
int             cjson_cmp           (cjson_ptr a, cjson_ptr b);
int             cjson_eq            (cjson_ptr a, cjson_ptr b);
size_t          cjson_count         (cjson_ptr ptr);
int             cjson_as_int        (cjson_ptr ptr);
float           cjson_as_float      (cjson_ptr ptr);
void *          cjson_data          (cjson_ptr ptr);
enum cjson_type cjson_get_type      (cjson_ptr ptr);

int             cjson_str_cmp       (cjson_ptr ptr, char * str);

cjson_ptr       cjson_array_get     (cjson_ptr arr, size_t index);

cjson_ptr       cjson_obj_get       (cjson_ptr obj, char * key);
int             cjson_obj_has_key   (cjson_ptr obj, char * key);







#endif