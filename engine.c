#include "engine.h"

#include <assert.h>
#include "json.h"

enum jsondb_engine_action {
    JSONDB_ENGINE_INSERT,
};



typedef cjson_ptr jsondb_engine_insert_data;


enum jsondb_engine_action extract_action(cjson_ptr cmd) {
    cjson_ptr value;
    size_t action_size;
    char * action_mem;

    value = cjson_obj_get(cmd, "action");
    assert(value);
    assert(cjson_get_type(value) == CJSON_STR);

    if(cjson_str_cmp(value, "insert") == 0) {
        return JSONDB_ENGINE_INSERT;
    }

}

jsondb_engine_insert_data extract_insert_data(cjson_ptr cmd) {
    cjson_ptr value;

    value = cjson_obj_get(cmd, "data");
    assert(value);

    return value;
}

struct jsondb_set jsondb_engine_exec(char * _cmd) {
    cjson_ptr cmd;
    struct jsondb_set result = JSONDB_SET_EMPTY;
    enum jsondb_engine_action action;

    jsondb_engine_insert_data insert_data;

    cmd = cjson_load(_cmd);

    action = extract_action(cmd);
    assert(action >= 0);

    switch(action) {
    case JSONDB_ENGINE_INSERT: {
        /* insert some value into the db. very cool */
        insert_data = extract_insert_data(cmd);
        assert(insert_data != NULL);
        
        /* add the value to the result and main db set */
        jsondb_set_add_cjson(&result, insert_data); /* very cool! */
        jsondb_add_cjson(insert_data);
    } break;
    }

    return result;
}