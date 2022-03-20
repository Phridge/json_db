#include <stdio.h>
#include "jsondb.h"
#include "server.h"

int main(void) {
    struct jsondb_server_msg * msg;
    jsondb_server_init();

    msg = jsondb_server_poll(10000);

    jsondb_server_deinit();
    return 0;
}

