#include "engine.h"

#include <assert.h>
#include "json.h"

enum jsondb_engine_action {
    JSONDB_ENGINE_INSERT,
};



typedef char * jsondb_engine_insert_data;

static int extract_action(struct json_head * head) {
    char name_buf[16];
    enum json_sig type;
    enum jsondb_engine_action action;
    json_mark mark;

    json_get(head, "m", &mark); /* to restore the position later */
    json_next(head);

    while(1) {
        json_get(head, "t", &type);
        
        if(type == JSON_ERROR) {
            /* then we need to handle the error! todo */
            assert(0);
        } else if(type == JSON_END) {
            goto error;
        } else {
            /* get key value */
            json_get(head, "s.", name_buf, 16);
            
            if(strcmp(name_buf, "action") != 0) {
                json_skip(head);
                continue;
            }
            
            /* what is the action? a string */
            json_get(head, "t", &type);
            if(type != JSON_STR) {
                json_skip(head);
                continue;
            }

            /* get string value */
            json_get(head, "s.", name_buf, 16);
            if(strcmp(name_buf, "insert") == 0) {
                action = JSONDB_ENGINE_INSERT;
                goto success;
            } else {
                /* not yet covered/doesn't exist */
                goto error;
            }
        }
    }

error:
    json_restore(head, &mark);
    return -1;
success:
    json_restore(head, &mark);
    return action;
}


static jsondb_engine_insert_data extract_insert_data(struct json_head * head) {
    /* goal: look for "data" key and return begin of value */
    enum json_sig type;
    char name_buf[5]; /* just enough for "data" */
    char * begin;

    json_next(head); /* skip JSON_OBJ */

    while(1) {
        json_get(head, "t", &type);
        if(type == JSON_END) {
            return NULL;
        }

        assert(type == JSON_STR);

        /* check if the object key is "data" */
        json_get(head, "s.", name_buf, 5);
        if(strcmp(name_buf, "data") == 0) {
            /* now lets get the begin of the value! */
            json_get(head, "[", &begin);
            return begin;
        } else {
            json_skip(head);
        }

    }
}


struct jsondb_set jsondb_engine_exec(char *cmd) {
    struct json_head head;
    enum json_ctx stack[32]; /* good enough */
    enum jsondb_engine_action action;
    jsondb_engine_insert_data insert_data;
    
    struct jsondb_set result = JSONDB_SET_EMPTY;

    json_init(&head, cmd, stack, 32);

    /* find what to do "action":"<what action?>" */
    action = extract_action(&head);
    assert(action >= 0);

    switch(action) {
    case JSONDB_ENGINE_INSERT: {
        /* insert some value into the db. very cool */
        insert_data = extract_insert_data(&head);
        assert(insert_data != NULL);
        jsondb_set_add(&result, insert_data); /* very cool! */
        jsondb_add_set(&result);
    } break;
    }

    return result;
}