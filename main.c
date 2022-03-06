#include <stdio.h>
#include "jsondb.h"

int main(void) {
    jsondb_init();

    jsondb_insert("{\"Hello\": [1,2,-3.45e-8,true,false,null, {}, [], \"\\n\"]}", NULL);

    jsondb_deinit();
    return 0;
}

