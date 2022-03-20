#ifndef _SERVER_H_
#define _SERVER_H_

#include <stdlib.h>

/* Server application for the DB.
 * Uses/Supports Protocols:
 * - idk
 * Basically, there comes a json document in, and there goes one out.
 * Where can you find something that simple? Make own Protocol on top of TCP?
 */


int jsondb_server_init(void);

struct jsondb_server_msg {
    char * buf;
    size_t size;
};

struct jsondb_server_msg * jsondb_server_poll(int timeout_ms);

void jsondb_server_deinit(void);

#endif