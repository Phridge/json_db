#ifndef _JSONDB_H_
#define _JSONDB_H_


/* Different JSONDB-Internal Types */
enum jsondb_types {
    JSONDB_TYPE_OBJECT,
    JSONDB_TYPE_ARRAY,
    JSONDB_TYPE_STR,
    JSONDB_TYPE_F64,
    JSONDB_TYPE_I64,
    JSONDB_TYPE_TRUE,
    JSONDB_TYPE_FALSE,
    JSONDB_TYPE_NULL,
};

/* Database entry 
 * Organized in a linked list */
struct jsondb_entry {
    struct jsondb_entry * next, * prev, * template;
};

/* Pointer to the value part of an entry */
typedef void * jsondb_value;

/* get the data part of an entry */
#define JSONDB_ENTRY_DATA(entry) ((void*)(entry) + sizeof(entry))

/* Describes an iterator over a subset of the database. */
struct jsondb_sset {
    struct jsondb_entry * ptr;
};

/* Fetches the next entry from a subset. */
void jsondb_next(struct jsondb_sset * sset);


/* Insert a Text-JSON document */
void jsondb_insert(char * begin, char * end);

/* Query for documents that have a certain value at a path
 * https://datatracker.ietf.org/doc/html/rfc6901 */
struct jsondb_sset jsondb_search(char * path, jsondb_value value);





#endif